consts = require 'consts'

{ Stack } = require 'stack'

llvm = require 'llvm'
irbuilder = llvm.IRBuilder

#
# ExitableScopes are basically the means by which ejs deals with 'break' and 'continue'.
# 
# Each ExitableScope has two exit functions, exitFore and exitAft.
# exitFore corresponds to 'continue', and exitAft corresponds to
# 'break' (or falling off the end of the scope if the fromBreak arg is false.)
#
exports.ExitableScope = class ExitableScope
        @REASON_RETURN: -10
        
        @scopeStack: null
        
        constructor: (@label = null) ->
                @parent = null

        exitFore: (label = null) ->
                throw "Exitable scope doesn't allow exitFore"

        exitAft: (fromBreak, label = null) ->
                throw "Exitable scope doesn't allow exitAft"

        enter: ->
                @parent = ExitableScope.scopeStack
                ExitableScope.scopeStack = @

        leave: ->
                ExitableScope.scopeStack = @parent
                @parent = null
                
exports.TryExitableScope = class TryExitableScope extends ExitableScope
        @REASON_FALLOFF_TRY:    -2  # we fell off the end of the try block
        @REASON_ERROR:          -1  # error condition

        @REASON_BREAK: "break"
        @REASON_CONTINUE: "continue"
        
        @unwindStack: new Stack
                
        constructor: (@cleanup_reason, @cleanup_bb, @create_landing_pad_bb, @hasFinally) ->
                @isTry = true
                @destinations = []
                super()

        enter: ->
                TryExitableScope.unwindStack.push @
                super
                
        leave: ->
                TryExitableScope.unwindStack.pop()
                super

        getLandingPadBlock: ->
                return @landing_pad_block if @landing_pad_block?
                @landing_pad_block = @create_landing_pad_bb()
                
        
        lookupDestinationIdForScope: (scope, reason) ->
                for i in [0...@destinations.length]
                        if @destinations[i].scope is scope and @destinations[i].reason is reason
                                return @destinations[i].id

                id = consts.int32 @destinations.length
                @destinations.unshift scope: scope, reason: reason, id: id
                id
                
        
        exitFore: (label = null) ->
                if label?
                        scope = LoopExitableScope.findLabeledOrFinally label, @parent
                else
                        scope = LoopExitableScope.findLoopOrFinally @parent

                if @hasFinally
                        reason = @lookupDestinationIdForScope scope, TryExitableScope.REASON_CONTINUE 
                        irbuilder.createStore reason, @cleanup_reason
                        irbuilder.createBr @cleanup_bb
                else
                        scope.exitFore()

        exitAft: (fromBreak, label = null) ->
                # first we find our destination scope
                if fromBreak
                        if label?
                                scope = LoopExitableScope.findLabeledOrFinally label, @parent
                        else
                                scope = @parent

                # then we either create a branch to our cleanup_bb
                # with the right reason (we'll encode the exitAft from
                # the dest scope in the cleanup_bb), or we exit from
                # the dest scope directly if we're lacking a cleanup_bb
                if @hasFinally
                        if fromBreak
                                reason = @lookupDestinationIdForScope scope, TryExitableScope.REASON_BREAK
                        else
                                reason = consts.int32 TryExitableScope.REASON_FALLOFF_TRY

                        irbuilder.createStore reason, @cleanup_reason
                        irbuilder.createBr @cleanup_bb
                else
                        if fromBreak
                                scope.exitAft fromBreak
                        else
                                irbuilder.createBr @cleanup_bb

exports.SwitchExitableScope = class SwitchExitableScope extends ExitableScope
        constructor: (@merge_bb) ->
                super()

        exitAft: (fromBreak, label = null) ->
                irbuilder.createBr @merge_bb

exports.LoopExitableScope = class LoopExitableScope extends ExitableScope
        constructor: (label, @fore_bb, @aft_bb) ->
                @isLoop = true
                super label

        exitFore: (label = null) ->
                if label? and label isnt @label
                        (LoopExitableScope.findLabeledOrFinally label).exitFore label
                else
                        irbuilder.createBr @fore_bb
                
        exitAft: (fromBreak, label = null) ->
                if label? and label isnt @label
                        (LoopExitableScope.findLabeledOrFinally label).exitAft label
                else
                        irbuilder.createBr @aft_bb

        @findLabeledOrFinally: (l, stack = ExitableScope.scopeStack) ->
                return stack if l is stack.label
                return stack if stack.hasFinally
                return LoopExitableScope.findLabeledOrFinally l, stack.parent

        @findLoopOrFinally: (stack = ExitableScope.scopeStack) ->
                return stack if stack.isLoop
                return stack if stack.hasFinally
                return LoopExitableScope.findLoopOrFinally stack.parent

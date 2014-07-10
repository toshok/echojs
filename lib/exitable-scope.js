import { Stack } from './stack-es6'

let llvm = require('llvm');
let irbuilder = llvm.IRBuilder;

module consts from './consts';


//
// ExitableScopes are basically the means by which ejs deals with 'break' and 'continue'.
// 
// Each ExitableScope has two exit functions, exitFore and exitAft.
// exitFore corresponds to 'continue', and exitAft corresponds to
// 'break' (or falling off the end of the scope if the fromBreak arg is false.)
//
export class ExitableScope {
    constructor (label = null) {
	this.label = label;
        this.parent = null;
    }

    exitFore (label = null) {
        throw new Error("Exitable scope doesn't allow exitFore");
    }

    exitAft (fromBreak, label = null) {
        throw new Error("Exitable scope doesn't allow exitAft");
    }

    enter () {
        this.parent = ExitableScope.scopeStack;
        ExitableScope.scopeStack = this;
    }

    leave () {
        ExitableScope.scopeStack = this.parent;
        this.parent = null;
    }
}
ExitableScope.scopeStack = null;
ExitableScope.REASON_RETURN = -10;
        
                
export class TryExitableScope extends ExitableScope {
    constructor (cleanup_reason, cleanup_bb, create_landing_pad_bb, hasFinally) {
	this.cleanup_reason = cleanup_reason;
	this.cleanup_bb = cleanup_bb;
	this.create_landing_pad_bb = create_landing_pad_bb;
	this.hasFinally = hasFinally;
        this.isTry = true;
        this.destinations = [];
        super();
    }

    enter() {
        TryExitableScope.unwindStack.push(this);
        super();
    }
                
    leave() {
        TryExitableScope.unwindStack.pop();
        super();
    }

    getLandingPadBlock() {
        if (!this.landing_pad_block)
	    this.landing_pad_block = this.create_landing_pad_bb();
	return this.landing_pad_block;
    }
                
        
    lookupDestinationIdForScope (scope, reason) {
	for (let dest of this.destinations)
            if (dest.scope === scope && dest.reason === reason)
                return dest.id;

        let id = consts.int32(this.destinations.length);
        this.destinations.unshift ({scope: scope, reason: reason, id: id });
        return id;
    }
                
        
    exitFore (label = null) {
	let scope;
        if (label)
            scope = LoopExitableScope.findLabeledOrFinally(label, this.parent);
        else
            scope = LoopExitableScope.findLoopOrFinally(this.parent);

        if (this.hasFinally) {
            let reason = this.lookupDestinationIdForScope(scope, TryExitableScope.REASON_CONTINUE );
            irbuilder.createStore(reason, this.cleanup_reason);
            irbuilder.createBr(this.cleanup_bb);
	}
        else {
            scope.exitFore();
	}
    }

    exitAft (fromBreak, label = null) {
	let scope;
        // first we find our destination scope
        if (fromBreak) {
            if (label)
                scope = LoopExitableScope.findLabeledOrFinally(label, this.parent);
            else
                scope = this.parent;
	}

        // then we either create a branch to our cleanup_bb
        // with the right reason (we'll encode the exitAft from
        // the dest scope in the cleanup_bb), or we exit from
        // the dest scope directly if we're lacking a cleanup_bb
        if (this.hasFinally) {
	    let reason;
            if (fromBreak)
                reason = this.lookupDestinationIdForScope(scope, TryExitableScope.REASON_BREAK);
            else
                reason = consts.int32(TryExitableScope.REASON_FALLOFF_TRY);

            irbuilder.createStore(reason, this.cleanup_reason);
            irbuilder.createBr(this.cleanup_bb);
	}
        else {
            if (fromBreak)
                scope.exitAft(fromBreak);
            else
                irbuilder.createBr(this.cleanup_bb);
	}
    }
}
TryExitableScope.REASON_FALLOFF_TRY =    -2;  // we fell off the end of the try block
TryExitableScope.REASON_ERROR =          -1;  // error condition
TryExitableScope.REASON_BREAK = "break";
TryExitableScope.REASON_CONTINUE = "continue";
TryExitableScope.unwindStack = new Stack;
                

export class SwitchExitableScope extends ExitableScope {
    constructor (merge_bb) {
	this.merge_bb = merge_bb;
        super();
    }

    exitAft (fromBreak, label = null) {
        irbuilder.createBr(this.merge_bb);
    }
}

export class LoopExitableScope extends ExitableScope {
    constructor (label, fore_bb, aft_bb) {
	this.fore_bb = fore_bb;
	this.aft_bb = aft_bb;
        this.isLoop = true;
        super(label);
    }

    exitFore (label = null) {
        if (label && label !== this.label)
            LoopExitableScope.findLabeledOrFinally(label).exitFore(label);
        else
            irbuilder.createBr(this.fore_bb);
    }
                
    exitAft (fromBreak, label = null) {
        if (label && label !== this.label)
            LoopExitableScope.findLabeledOrFinally(label).exitAft(label);
        else
            irbuilder.createBr(this.aft_bb);
    }

    static findLabeledOrFinally (l, stack = ExitableScope.scopeStack) {
        if (l === stack.label) return stack;
        if (stack.hasFinally)  return stack;
        return LoopExitableScope.findLabeledOrFinally(l, stack.parent);
    }

    static findLoopOrFinally (stack = ExitableScope.scopeStack) {
        if (stack.isLoop) return stack;
        if (stack.hasFinally) return stack;
        return LoopExitableScope.findLoopOrFinally(stack.parent);
    }
}

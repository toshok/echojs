terminal = require 'terminal'

red = -> terminal.ANSIStyle('red')
yellow = -> terminal.ANSIStyle('yellow')
green = -> terminal.ANSIStyle('green')
reset = ->terminal.ANSIStyle('default')

num_asserts = 0
num_failures = 0
num_xfails = 0

exports.XFAIL = (str = "") -> { flag: true, reason: str }

exports.eq = (id, val, expected, xfail) ->
                num_asserts += 1
                if val isnt expected
                        console.log "#{if xfail?.flag then yellow() else red()}#{if xfail then 'XFAIL' else 'ERROR'}#{reset()}: #{id}: expected `#{expected}', but got `#{val}'.  #{if xfail?.flag then yellow()+xfail.reason+reset() else ''}"
                        if xfail?.flag
                                num_xfails += 1
                        else
                                num_failures += 1
                else
                        if xfail?.flag
                                console.log "#{red()}ERROR#{reset()}: #{id}: unexpected success"
                                num_failures += 1
                        else
                                console.log "#{green()}SUCCESS#{reset()}: #{id}: passed"

exports.dumpStats = ->
                console.log "#{if num_failures > 0 then red() else green()}#{num_asserts} asserts, #{num_failures} failures, #{num_xfails} expected failures.#{reset()}"


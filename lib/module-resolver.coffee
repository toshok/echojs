path = require 'path'
fs = require 'fs'
esprima = require 'esprima'
escodegen = require 'escodegen'
{ is_string_literal } = require 'echo-util'
{ TreeVisitor } = require 'nodevisitor'

b = require 'ast-builder'

parseFile = (filename, content) ->
        try
                parse_tree = esprima.parse content, { loc: true, raw: true }
        catch e
                console.warn "#{filename}: #{e}:"
                process.exit -1

        return parse_tree
        
# this class does two things
#
# 1. rewrites all sources to be relative to @toplevel_path.  i.e. if
#    the following directory structure exists:
#
#    externals/
#      ext1.js
#    root/
#      main.js      (contains: import { foo } from "modules/foo" )
#      modules/
#        foo1.js    (contains: module ext1 from "../../externals/ext1")
#
#    $PWD = root/
#
#      $ ejs main.js
#
#    ejs will rewrite module paths such that main.js is unchanged, and
#    foo1.js's module declaration reads:
#
#      "../externals/ext1"
# 
# 2. builds up a list (@importList) containing the list of all
#    imported modules
#
class GatherImports extends TreeVisitor
        constructor: (@path, @toplevel_path) ->
                @importList = []

        isInternalModule = (source) -> source[0] is "@"
                
        addSource: (n) ->
                return n if not n.source?
                
                throw new Error("import sources must be strings") if not is_string_literal(n.source)

                if isInternalModule(n.source.value)
                        n.source_path = b.literal(n.source_value)
                        return n
                
                if n.source[0] is "/"
                        source_path = n.source.value
                else
                        source_path = path.resolve @toplevel_path, "#{@path}/#{n.source.value}"

                if source_path.indexOf(process.cwd()) is 0
                        source_path = path.relative(process.cwd(), source_path)
                        
                @importList.push(source_path) if @importList.indexOf(source_path) is -1

                n.source_path = b.literal(source_path)
                n
        
        visitImportDeclaration: (n) -> @addSource(n)
        visitExportDeclaration: (n) -> @addSource(n)
        visitModuleDeclaration: (n) -> @addSource(n)

gatherImports = (filename, top_path, tree) ->
        visitor = new GatherImports(filename, top_path)
        visitor.visit(tree)
        visitor.importList

# module references that ejs supports:
#
#   file references, e.g.:   'foo' (imports foo.js), "../bar/baz" (imports ../bar/baz.js relative to the file containing the reference)
#   builtin modules, e.g.:   '@reflect' (though there is some overlap with file references due to node-compat)
#
# for unresolved modules, the string is passed to a callback, which can do whatever it likes to try to resolve the module.
# its signature is  (importingModule:string, moduleRef:string, callback:(err, {  [optional] path: string, [optional] ast: parsed-ast }))
#
# if a resolver can't resolve a module (the module ref is not valid for this resolver), it should invoke the callback with (null, null).
# if there is an exception/error during fetching/parsing a module, it should invoke the callback with (error, null).
# if the module is sucessfully fetched/parsed, it should invoke the callback with (null, result).

defaultResolver = (importingModuleRef, moduleRef, callback) ->

        handleFileExists = (path) ->
                try
                        # XXX we need to resolve the path to moduleRef relative to importingModuleRef
                        module_contents = fs.readFileSync(path, 'utf-8')
                        module_ast      = parseFile(moduleRef, module_contents)

                        callback null, { path: moduleRef, ast: module_ast }
                catch e
                        callback e, null
                
        fs.exists moduleRef, (exists) ->
                return handleFileExists(moduleRef) if exists
                        
                moduleRef += ".js"
                fs.exists moduleRef, (exists) ->
                        return handleFileExists(moduleRef) if exists
                        callback(null, null)

resolver_list = [defaultResolver]

resolveModule = (importingModuleRef, moduleRef, callback) ->
        current_resolver = 0
        doResolve = (err, result) ->
                if not err and not result
                        current_resolver += 1
                        if current_resolver is resolver_list.length
                                # we've reached the end of the list without finding the module
                                return callback(new Error("failed to resolve module ref '#{moduleRef}' from '#{importingModuleRef}'"), null)
                                
                        return resolver_list[current_resolver] importingModuleRef, moduleRef, doResolve

                callback(err, result)

        # get the process started
        resolver_list[current_resolver] importingModuleRef, moduleRef, doResolve


exports.addResolver = (resolver) -> resolver_list.unshift(resolver)

exports.resolveModules = (work_list, callback) ->
        files = []

        # starting at the main file, gather all files we'll need

        resolveIter = (err) ->
                return callback(err, null) if err?
                
                if work_list.length is 0
                        return callback(null, files)

                moduleref = work_list.pop()

                # need to pass the importingModuleRef here clearly...
                resolveModule null, moduleref, (err, result) ->
                        throw err if err?

                        file_info = {
                                file_name: result.path || moduleref,
                                file_ast: result.ast || parseFile(moduleref, result.path)
                        }
        
                        files.push(file_info)

                        ## this needs reworking
                        imports = gatherImports(path.dirname(file_info.file_name), process.cwd(), file_info.file_ast)

                        for i in imports
                                if work_list.indexOf(i) is -1 and not files.some((el) -> el.file_name is i)
                                        work_list.push(i)

                        resolveIter()


        resolveIter()
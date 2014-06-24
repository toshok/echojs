%0 = type { i8*, i32 }
%EjsValueType = type { i64 }
%EjsExceptionTypeInfoType = type { i8*, i8*, i8* }
@EJS_EHTYPE_ejsvalue = external global %EjsExceptionTypeInfoType*

define i32 @_ejs_invoke_closure_catch (%EjsValueType* nocapture %retval, %EjsValueType %closure, %EjsValueType %_this, i32 %argc, %EjsValueType* nocapture readnone %args) {
entry:
  %rv_alloc = alloca i32
  %call = alloca %EjsValueType

  %ref = getelementptr inbounds %EjsValueType* %retval, i64 0, i32 0

  invoke void @_ejs_invoke_closure(i64* %ref, %EjsValueType %closure, %EjsValueType %_this, i32 %argc, %EjsValueType* %args)
          to label %success unwind label %exception

success:

  store i32 1, i32* %rv_alloc

  br label %try_merge

exception:
  %caught_result = landingpad %0 personality i8* bitcast (i32 (i32, i32, i64, i8*, i8*)* @__ejs_personality_v0 to i8*)
          cleanup
          catch i8* bitcast (%EjsExceptionTypeInfoType** @EJS_EHTYPE_ejsvalue to i8*)
  %exception4 = extractvalue %0 %caught_result, 0
  call void @_ejs_begin_catch(i64* %ref, i8* %exception4)

  store i32 0, i32* %rv_alloc

  call void @_ejs_end_catch()

  br label %try_merge

try_merge:
  %rvload = load i32* %rv_alloc
  ret i32 %rvload
}

declare void @_ejs_invoke_closure(i64*, %EjsValueType, %EjsValueType, i32, %EjsValueType*)

declare i32 @__ejs_personality_v0(i32, i32, i64, i8*, i8*)

declare void @_ejs_begin_catch(i64*, i8*)
declare void @_ejs_end_catch()

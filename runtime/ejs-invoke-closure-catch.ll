%0 = type { i8*, i32 }
%EjsValueType = type { i64 }
%EjsExceptionTypeInfoType = type { i8*, i8*, i8* }
@EJS_EHTYPE_ejsvalue = external global %EjsExceptionTypeInfoType*

%EjsFuncType = type {  }

define i32 @_ejs_invoke_closure_catch (%EjsValueType* nocapture %retval, %EjsValueType %closure, %EjsValueType* %_this, i32 %argc, %EjsValueType* nocapture readnone %args, i32 %callFlags, %EjsValueType %newTarget) {
entry:
  %rv_alloc = alloca i32

  %ref = getelementptr inbounds %EjsValueType* %retval, i64 0, i32 0

  %call = invoke i64 @_ejs_invoke_closure(%EjsValueType %closure, %EjsValueType* %_this, i32 %argc, %EjsValueType* %args, i32 %callFlags, %EjsValueType %newTarget)
          to label %success unwind label %exception

success:

  store i64 %call, i64* %ref
  store i32 1, i32* %rv_alloc

  br label %try_merge

exception:
  %caught_result = landingpad %0 personality i8* bitcast (i32 (i32, i32, i64, i8*, i8*)* @__ejs_personality_v0 to i8*)
          cleanup
          catch i8* bitcast (%EjsExceptionTypeInfoType** @EJS_EHTYPE_ejsvalue to i8*)
  %exception4 = extractvalue %0 %caught_result, 0
  %begincatch = call i64 @_ejs_begin_catch(i8* %exception4)

  store i64 %begincatch, i64* %ref
  store i32 0, i32* %rv_alloc

  call void @_ejs_end_catch()

  br label %try_merge

try_merge:
  %rvload = load i32* %rv_alloc
  ret i32 %rvload
}

define i32 @_ejs_invoke_func_catch (%EjsValueType* nocapture %retval, i64 (i8*)* %func, i8* %data) {
entry:
  %rv_alloc = alloca i32

  %ref = getelementptr inbounds %EjsValueType* %retval, i64 0, i32 0

  %call = invoke i64 %func(i8* %data)
          to label %success unwind label %exception

success:

  store i64 %call, i64* %ref
  store i32 1, i32* %rv_alloc

  br label %try_merge

exception:
  %caught_result = landingpad %0 personality i8* bitcast (i32 (i32, i32, i64, i8*, i8*)* @__ejs_personality_v0 to i8*)
          cleanup
          catch i8* bitcast (%EjsExceptionTypeInfoType** @EJS_EHTYPE_ejsvalue to i8*)
  %exception4 = extractvalue %0 %caught_result, 0
  %begincatch = call i64 @_ejs_begin_catch(i8* %exception4)

  store i64 %begincatch, i64* %ref
  store i32 0, i32* %rv_alloc

  call void @_ejs_end_catch()

  br label %try_merge

try_merge:
  %rvload = load i32* %rv_alloc
  ret i32 %rvload
}

declare i64 @_ejs_invoke_closure(%EjsValueType, %EjsValueType*, i32, %EjsValueType*, i32, %EjsValueType)

declare i32 @__ejs_personality_v0(i32, i32, i64, i8*, i8*)

declare i64 @_ejs_begin_catch(i8*)
declare void @_ejs_end_catch()

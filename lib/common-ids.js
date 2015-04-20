/* -*- Mode: js2; tab-width: 4; indent-tabs-mode: nil; -*-
 * vim: set ts=4 sw=4 et tw=99 ft=js:
 */

import { identifier } from './ast-builder';

export const superid                 = identifier("%super");
export const makeClosureEnv_id       = identifier("%makeClosureEnv");
export const makeClosure_id          = identifier("%makeClosure");
export const makeClosureNoEnv_id     = identifier("%makeClosureNoEnv");
export const makeAnonClosure_id      = identifier("%makeAnonClosure");
export const makeGenerator_id        = identifier("%makeGenerator");
export const generatorYield_id       = identifier("%generatorYield");
export const setSlot_id              = identifier("%setSlot");
export const slot_id                 = identifier("%slot");
export const invokeClosure_id        = identifier("%invokeClosure");
export const setLocal_id             = identifier("%setLocal");
export const setGlobal_id            = identifier("%setGlobal");
export const getLocal_id             = identifier("%getLocal");
export const getGlobal_id            = identifier("%getGlobal");
export const moduleGet_id            = identifier("%moduleGet");
export const moduleGetSlot_id        = identifier("%moduleGetSlot");
export const moduleSetSlot_id        = identifier("%moduleSetSlot");
export const moduleGetExotic_id      = identifier("%moduleGetExotic");
export const templateCallsite_id     = identifier("%templateCallsite");
export const templateHandlerCall_id  = identifier("%templateHandlerCall");
export const templateDefaultHandlerCall_id = identifier("%templateDefaultHandlerCall");
export const createArgScratchArea_id       = identifier("%createArgScratchArea");
export const setPrototypeOf_id       = identifier("%setPrototypeOf");
export const objectCreate_id         = identifier("%objectCreate");
export const arrayFromRest_id        = identifier("%arrayFromRest");
export const arrayFromSpread_id      = identifier("%arrayFromSpread");
export const builtinUndefined_id     = identifier("%builtinUndefined");

export const typeofIsObject_id       = identifier("%typeofIsObject");
export const typeofIsFunction_id     = identifier("%typeofIsFunction");
export const typeofIsString_id       = identifier("%typeofIsString");
export const typeofIsSymbol_id       = identifier("%typeofIsSymbol");
export const typeofIsNumber_id       = identifier("%typeofIsNumber");
export const typeofIsBoolean_id      = identifier("%typeofIsBoolean");
export const isNull_id               = identifier("%isNull");
export const isUndefined_id          = identifier("%isUndefined");
export const isNullOrUndefined_id    = identifier("%isNullOrUndefined");

export const argPresent_id           = identifier("%argPresent");
export const getArg_id               = identifier("%getArg");
export const getArgumentsObject_id   = identifier("%getArgumentsObject");

export const createIterResult_id     = identifier("%createIterResult");

export const Symbol_id = identifier("Symbol");
export const iterator_id = identifier("iterator");
export const value_id = identifier("value");
export const next_id = identifier("next");
export const done_id = identifier("done");
export const prototype_id = identifier("prototype");
export const proto_id = identifier("proto");
export const constructor_id = identifier("constructor");
export const call_id = identifier("call");
export const apply_id = identifier("apply");
export const Object_id = identifier("Object");
export const defineProperty_id = identifier("defineProperty");
export const defineProperties_id = identifier("defineProperties");
export const get_id = identifier("get");
export const set_id = identifier("set");
export const enumerable_id = identifier("enumerable");
export const env_unused_id = identifier('%env_unused');

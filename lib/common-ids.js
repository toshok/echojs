/* -*- Mode: js2; tab-width: 4; indent-tabs-mode: nil; -*-
 * vim: set ts=4 sw=4 et tw=99 ft=js:
 */

module b from './ast-builder';

export const superid                 = b.identifier("%super");
export const makeClosureEnv_id       = b.identifier("%makeClosureEnv");
export const makeClosure_id          = b.identifier("%makeClosure");
export const makeAnonClosure_id      = b.identifier("%makeAnonClosure");
export const setSlot_id              = b.identifier("%setSlot");
export const slot_id                 = b.identifier("%slot");
export const invokeClosure_id        = b.identifier("%invokeClosure");
export const setLocal_id             = b.identifier("%setLocal");
export const setGlobal_id            = b.identifier("%setGlobal");
export const getLocal_id             = b.identifier("%getLocal");
export const getGlobal_id            = b.identifier("%getGlobal");
export const moduleGet_id            = b.identifier("%moduleGet");
export const moduleImportBatch_id    = b.identifier("%moduleImportBatch");
export const templateCallsite_id     = b.identifier("%templateCallsite");
export const templateHandlerCall_id  = b.identifier("%templateHandlerCall");
export const templateDefaultHandlerCall_id = b.identifier("%templateDefaultHandlerCall");
export const createArgScratchArea_id       = b.identifier("%createArgScratchArea");
export const setPrototypeOf_id       = b.identifier("%setPrototypeOf");
export const objectCreate_id         = b.identifier("%objectCreate");
export const gatherRest_id           = b.identifier("%gatherRest");
export const arrayFromSpread_id      = b.identifier("%arrayFromSpread");
export const builtinUndefined_id     = b.identifier("%builtinUndefined");

export const typeofIsObject_id   = b.identifier("%typeofIsObject");
export const typeofIsFunction_id = b.identifier("%typeofIsFunction");
export const typeofIsString_id   = b.identifier("%typeofIsString");
export const typeofIsSymbol_id   = b.identifier("%typeofIsSymbol");
export const typeofIsNumber_id   = b.identifier("%typeofIsNumber");
export const typeofIsBoolean_id  = b.identifier("%typeofIsBoolean");
export const isNull_id           = b.identifier("%isNull");
export const isUndefined_id      = b.identifier("%isUndefined");

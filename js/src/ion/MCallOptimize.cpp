/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=79:
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla SpiderMonkey JavaScript (IonMonkey).
 *
 * The Initial Developer of the Original Code is
 *   Nicolas Pierron <nicolas.b.pierron@mozilla.com>
 * Portions created by the Initial Developer are Copyright (C) 2012
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Nicolas B. Pierron <nicolas.b.pierron@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "jslibmath.h"
#include "jsmath.h"

#include "MIR.h"
#include "MIRGraph.h"
#include "IonBuilder.h"

namespace js {
namespace ion {

bool
IonBuilder::discardCallArgs(uint32 argc, MDefinitionVector &argv, MBasicBlock *bb)
{
    if (!argv.resizeUninitialized(argc + 1))
        return false;

    // Bytecode order: Function, This, Arg0, Arg1, ..., ArgN, Call.
    // Copy PassArg arguments from ArgN to This.
    for (int32 i = argc; i >= 0; i--) {
        MPassArg *passArg = bb->pop()->toPassArg();
        MBasicBlock *block = passArg->block();
        MDefinition *wrapped = passArg->getArgument();
        passArg->replaceAllUsesWith(wrapped);
        block->discard(passArg);
        argv[i] = wrapped;
    }

    return true;
}

bool
IonBuilder::discardCall(uint32 argc, MDefinitionVector &argv, MBasicBlock *bb)
{
    if (!discardCallArgs(argc, argv, bb))
        return false;

    // Function MDefinition instruction implicitly consumed by inlining.
    bb->pop();

    return true;
}

IonBuilder::InliningStatus
IonBuilder::inlineMathFunction(MMathFunction::Function function, MIRType argType, MIRType returnType)
{
    if (!IsNumberType(argType))
        return InliningStatus_NotInlined;

    if (returnType != MIRType_Double)
        return InliningStatus_NotInlined;

    MDefinitionVector argv;
    if (!discardCall(/* argc = */ 1, argv, current))
        return InliningStatus_Error;

    MMathFunction *ins = MMathFunction::New(argv[1], function);
    current->add(ins);
    current->push(ins);
    return InliningStatus_Inlined;
}

IonBuilder::InliningStatus
IonBuilder::inlineNativeCall(JSFunction *target, uint32 argc, bool constructing)
{
    JSNative native = target->native();

    // Currently constructing is only possible with Array()
    if (constructing && native != js_Array)
        return InliningStatus_NotInlined;

    /* Check if there is a match for the current native function */

    types::TypeSet *barrier;
    types::TypeSet *returnTypes = oracle->returnTypeSet(script, pc, &barrier);
    MIRType returnType = MIRTypeFromValueType(returnTypes->getKnownTypeTag(cx));

    types::TypeSet *thisTypes = oracle->getCallArg(script, argc, 0, pc);
    MIRType thisType = MIRTypeFromValueType(thisTypes->getKnownTypeTag(cx));
    MDefinitionVector argv;

    if (argc == 0) {
        if (native == js_Array) {
            if (!discardCall(argc, argv, current))
                return InliningStatus_Error;
            types::TypeObject *type = types::TypeScript::InitObject(cx, script, pc, JSProto_Array);
            MNewArray *ins = new MNewArray(0, type, MNewArray::NewArray_Unallocating);
            current->add(ins);
            current->push(ins);
            if (!resumeAfter(ins))
                return InliningStatus_Error;
            return InliningStatus_Inlined;
        }

        if ((native == js::array_pop || native == js::array_shift) && thisType == MIRType_Object) {
            // Only handle pop/shift on dense arrays which have never been used
            // in an iterator --- when popping elements we don't account for
            // suppressing deleted properties in active iterators.
            //
            // Constraints propagating properties directly into the result
            // type set are generated by TypeConstraintCall during inference.
            if (!thisTypes->hasObjectFlags(cx, types::OBJECT_FLAG_NON_DENSE_ARRAY |
                                           types::OBJECT_FLAG_ITERATED) &&
                !types::ArrayPrototypeHasIndexedProperty(cx, script))
            {
                bool needsHoleCheck = thisTypes->hasObjectFlags(cx, types::OBJECT_FLAG_NON_PACKED_ARRAY);
                bool maybeUndefined = returnTypes->hasType(types::Type::UndefinedType());
                MArrayPopShift::Mode mode = (native == js::array_pop) ? MArrayPopShift::Pop : MArrayPopShift::Shift;
                JSValueType knownType = returnTypes->getKnownTypeTag(cx);
                if (knownType == JSVAL_TYPE_UNDEFINED || knownType == JSVAL_TYPE_NULL)
                    return InliningStatus_NotInlined;
                MIRType resultType = MIRTypeFromValueType(knownType);
                if (!discardCall(argc, argv, current))
                    return InliningStatus_Error;
                MArrayPopShift *ins = MArrayPopShift::New(argv[0], mode, needsHoleCheck, maybeUndefined);
                current->add(ins);
                current->push(ins);
                ins->setResultType(resultType);
                if (!resumeAfter(ins))
                    return InliningStatus_Error;
                return InliningStatus_Inlined;
            }
        }

        return InliningStatus_NotInlined;
    }

    types::TypeSet *arg1Types = oracle->getCallArg(script, argc, 1, pc);
    MIRType arg1Type = MIRTypeFromValueType(arg1Types->getKnownTypeTag(cx));

    if (argc == 1) {
        if (native == js_math_abs) {
            // argThis == MPassArg(MConstant(Math))
            if ((arg1Type == MIRType_Double || arg1Type == MIRType_Int32) &&
                arg1Type == returnType) {
                if (!discardCall(argc, argv, current))
                    return InliningStatus_Error;
                MAbs *ins = MAbs::New(argv[1], returnType);
                current->add(ins);
                current->push(ins);
                return InliningStatus_Inlined;
            }
        }
        if (native == js_math_sqrt) {
            // argThis == MPassArg(MConstant(Math))
            if ((arg1Type == MIRType_Double || arg1Type == MIRType_Int32) &&
                returnType == MIRType_Double) {
                if (!discardCall(argc, argv, current))
                    return InliningStatus_Error;
                MSqrt *ins = MSqrt::New(argv[1]);
                current->add(ins);
                current->push(ins);
                return InliningStatus_Inlined;
            }
        }
        if (native == js::math_log)
            return inlineMathFunction(MMathFunction::Log, arg1Type, returnType);
        if (native == js::math_sin)
            return inlineMathFunction(MMathFunction::Sin, arg1Type, returnType);
        if (native == js::math_cos)
            return inlineMathFunction(MMathFunction::Cos, arg1Type, returnType);
        if (native == js::math_tan)
            return inlineMathFunction(MMathFunction::Tan, arg1Type, returnType);
        if (native == js_math_floor) {
            // argThis == MPassArg(MConstant(Math))
            if (arg1Type == MIRType_Double && returnType == MIRType_Int32) {
                if (!discardCall(argc, argv, current))
                    return InliningStatus_Error;
                MFloor *ins = new MFloor(argv[1]);
                current->add(ins);
                current->push(ins);
                return InliningStatus_Inlined;
            }
            if (arg1Type == MIRType_Int32 && returnType == MIRType_Int32) {
                // i == Math.floor(i)
                if (!discardCall(argc, argv, current))
                    return InliningStatus_Error;
                current->push(argv[1]);
                return InliningStatus_Inlined;
            }
        }
        if (native == js_math_round) {
            // argThis == MPassArg(MConstant(Math))
            if (arg1Type == MIRType_Double && returnType == MIRType_Int32) {
                if (!discardCall(argc, argv, current))
                    return InliningStatus_Error;
                MRound *ins = new MRound(argv[1]);
                current->add(ins);
                current->push(ins);
                return InliningStatus_Inlined;
            }
            if (arg1Type == MIRType_Int32 && returnType == MIRType_Int32) {
                // i == Math.round(i)
                if (!discardCall(argc, argv, current))
                    return InliningStatus_Error;
                current->push(argv[1]);
                return InliningStatus_Inlined;
            }
        }
        // TODO: js_math_ceil

        // Compile any of the 3 because  charAt = fromCharCode . charCodeAt
        if (native == js_str_charCodeAt || native == js_str_charAt ||
            native == js::str_fromCharCode) {

            if (native == js_str_charCodeAt) {
                if (returnType != MIRType_Int32 || thisType != MIRType_String ||
                    arg1Type != MIRType_Int32) {
                    return InliningStatus_NotInlined;
                }
            }

            if (native == js_str_charAt) {
                if (returnType != MIRType_String || thisType != MIRType_String ||
                    arg1Type != MIRType_Int32) {
                    return InliningStatus_NotInlined;
                }
            }

            if (native == js::str_fromCharCode) {
                // argThis == MPassArg(MConstant(String))
                if (returnType != MIRType_String || arg1Type != MIRType_Int32)
                    return InliningStatus_NotInlined;
            }

            if (!discardCall(argc, argv, current))
                return InliningStatus_Error;
            MDefinition *str = argv[0];
            MDefinition *indexOrCode = argv[1];
            MInstruction *ins = MToInt32::New(indexOrCode);
            current->add(ins);
            indexOrCode = ins;

            if (native == js_str_charCodeAt || native == js_str_charAt) {
                MStringLength *length = MStringLength::New(str);
                MBoundsCheck *boundsCheck = MBoundsCheck::New(indexOrCode, length);
                current->add(length);
                current->add(boundsCheck);
                ins = MCharCodeAt::New(str, indexOrCode);
                current->add(ins);
                indexOrCode = ins;
            }
            if (native == js::str_fromCharCode || native == js_str_charAt) {
                ins = MFromCharCode::New(indexOrCode);
                current->add(ins);
            }
            current->push(ins);
            return InliningStatus_Inlined;
        }
        if (native == js_Array) {
            if (arg1Type == MIRType_Int32) {
                MDefinition *argv1 = current->peek(-1)->toPassArg()->getArgument();
                if (argv1->isConstant()) {
                    uint32 arg = argv1->toConstant()->value().toInt32();
                    // Negative arguments generate a RangeError, which is not
                    // handled by the inline path.
                    if (arg < JSObject::NELEMENTS_LIMIT) {
                        if (!discardCall(argc, argv, current))
                            return InliningStatus_Error;
                        types::TypeObject *type = types::TypeScript::InitObject(cx, script, pc, JSProto_Array);
                        MNewArray *ins = new MNewArray(arg, type, MNewArray::NewArray_Unallocating);
                        current->add(ins);
                        current->push(ins);
                        if (!resumeAfter(ins))
                            return InliningStatus_Error;
                        return InliningStatus_Inlined;
                    }
                }
            }
        }
        if (native == js::array_push) {
            // Constraints propagating properties into the 'this' object are
            // generated by TypeConstraintCall during inference.
            if (thisType == MIRType_Object && returnType == MIRType_Int32 &&
                !thisTypes->hasObjectFlags(cx, types::OBJECT_FLAG_NON_DENSE_ARRAY) &&
                !types::ArrayPrototypeHasIndexedProperty(cx, script))
            {
                if (!discardCall(argc, argv, current))
                    return InliningStatus_Error;
                MArrayPush *ins = MArrayPush::New(argv[0], argv[1]);
                current->add(ins);
                current->push(ins);
                if (!resumeAfter(ins))
                    return InliningStatus_Error;
                return InliningStatus_Inlined;
            }
        }

        return InliningStatus_NotInlined;
    }

    return InliningStatus_NotInlined;
}

} // namespace ion
} // namespace js

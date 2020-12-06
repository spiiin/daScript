#include "daScript/misc/platform.h"

#include "daScript/ast/ast.h"
#include "daScript/ast/ast_match.h"
#include "daScript/ast/ast_expressions.h"

#include "daScript/simulate/runtime_array.h"
#include "daScript/simulate/runtime_table_nodes.h"
#include "daScript/simulate/runtime_range.h"
#include "daScript/simulate/runtime_string_delete.h"
#include "daScript/simulate/hash.h"

#include "daScript/simulate/simulate_nodes.h"

#include "daScript/misc/lookup1.h"

#include "daScript/simulate/simulate_visit_op.h"

namespace das
{
    // common for move and copy

    SimNode * makeLocalCMResMove (const LineInfo & at, Context & context, uint32_t offset, const ExpressionPtr & rE ) {
        const auto & rightType = *rE->type;
        // now, call with CMRES
        if ( rE->rtti_isCall() ) {
            auto cll = static_pointer_cast<ExprCall>(rE);
            if ( cll->func->copyOnReturn || cll->func->moveOnReturn ) {
                SimNode_CallBase * right = (SimNode_CallBase *) rE->simulate(context);
                right->cmresEval = context.code->makeNode<SimNode_GetCMResOfs>(rE->at, offset);
                return right;
            }
        }
        // now, invoke with CMRES
        if ( rE->rtti_isInvoke() ) {
            auto cll = static_pointer_cast<ExprInvoke>(rE);
            if ( cll->isCopyOrMove() ) {
                SimNode_CallBase * right = (SimNode_CallBase *) rE->simulate(context);
                right->cmresEval = context.code->makeNode<SimNode_GetCMResOfs>(rE->at, offset);
                return right;
            }
        }
        // now, to the regular move
        auto left = context.code->makeNode<SimNode_GetCMResOfs>(at, offset);
        auto right = rE->simulate(context);
        if ( rightType.isRef() ) {
            return context.code->makeNode<SimNode_MoveRefValue>(at, left, right, rightType.getSizeOf());
        } else {
            DAS_ASSERTF(0, "we are calling makeLocalCMResMove where expression on a right is not a referece."
                        "we should not be here, script compiler should have caught this during compilation."
                        "compiler later will likely report internal compilation error.");
            return nullptr;
        }
    }

    SimNode * makeLocalCMResCopy(const LineInfo & at, Context & context, uint32_t offset, const ExpressionPtr & rE ) {
        const auto & rightType = *rE->type;
        assert ( rightType.canCopy() &&
                "we are calling makeLocalCMResCopy on a type, which can't be copied."
                "we should not be here, script compiler should have caught this during compilation."
                "compiler later will likely report internal compilation error.");
        auto right = rE->simulate(context);
        // now, call with CMRES
        if ( rE->rtti_isCall() ) {
            auto cll = static_pointer_cast<ExprCall>(rE);
            if ( cll->func->copyOnReturn || cll->func->moveOnReturn ) {
                SimNode_CallBase * rightC = (SimNode_CallBase *) right;
                rightC->cmresEval = context.code->makeNode<SimNode_GetCMResOfs>(rE->at, offset);
                return rightC;
            }
        }
        // now, invoke with CMRES
        if ( rE->rtti_isInvoke() ) {
            auto cll = static_pointer_cast<ExprInvoke>(rE);
            if ( cll->isCopyOrMove() ) {
                SimNode_CallBase * rightC = (SimNode_CallBase *) right;
                rightC->cmresEval = context.code->makeNode<SimNode_GetCMResOfs>(rE->at, offset);
                return rightC;
            }
        }
        // wo standard path
        auto left = context.code->makeNode<SimNode_GetCMResOfs>(rE->at, offset);
        if ( rightType.isHandle() ) {
            auto resN = rightType.annotation->simulateCopy(context, at, left,
                (!rightType.isRefType() && rightType.ref) ? rightType.annotation->simulateRef2Value(context, at, right) : right);
            if ( !resN ) {
                context.thisProgram->error("integration error, simulateCopy returned null", "", "",
                                           at, CompilationError::missing_node );
            }
            return resN;
        } else if ( rightType.isRef() ) {
            return context.code->makeNode<SimNode_CopyRefValue>(at, left, right, rightType.getSizeOf());
        } else {
            return context.code->makeValueNode<SimNode_Set>(rightType.baseType, at, left, right);
        }
    }

    SimNode * makeLocalRefMove (const LineInfo & at, Context & context, uint32_t stackTop, uint32_t offset, const ExpressionPtr & rE ) {
        const auto & rightType = *rE->type;
        // now, call with CMRES
        if ( rE->rtti_isCall() ) {
            auto cll = static_pointer_cast<ExprCall>(rE);
            if ( cll->func->copyOnReturn || cll->func->moveOnReturn ) {
                SimNode_CallBase * right = (SimNode_CallBase *) rE->simulate(context);
                right->cmresEval = context.code->makeNode<SimNode_GetLocalRefOff>(rE->at, stackTop, offset);
                return right;
            }
        }
        // now, invoke with CMRES
        if ( rE->rtti_isInvoke() ) {
            auto cll = static_pointer_cast<ExprInvoke>(rE);
            if ( cll->isCopyOrMove() ) {
                SimNode_CallBase * right = (SimNode_CallBase *) rE->simulate(context);
                right->cmresEval = context.code->makeNode<SimNode_GetLocalRefOff>(rE->at, stackTop, offset);
                return right;
            }
        }
        // now, to the regular move
        auto left = context.code->makeNode<SimNode_GetLocalRefOff>(at, stackTop, offset);
        auto right = rE->simulate(context);
        if ( rightType.isRef() ) {
            return context.code->makeNode<SimNode_MoveRefValue>(at, left, right, rightType.getSizeOf());
        } else {
            // this is typically lambda or handled non-copyable
            return context.code->makeValueNode<SimNode_Set>(rightType.baseType, at, left, right);
        }
    }

    SimNode * makeLocalRefCopy(const LineInfo & at, Context & context, uint32_t stackTop, uint32_t offset, const ExpressionPtr & rE ) {
        const auto & rightType = *rE->type;
        assert ( rightType.canCopy() &&
                "we are calling makeLocalRefCopy on a type, which can't be copied."
                "we should not be here, script compiler should have caught this during compilation."
                "compiler later will likely report internal compilation error.");
        auto right = rE->simulate(context);
        // now, call with CMRES
        if ( rE->rtti_isCall() ) {
            auto cll = static_pointer_cast<ExprCall>(rE);
            if ( cll->func->copyOnReturn || cll->func->moveOnReturn ) {
                SimNode_CallBase * rightC = (SimNode_CallBase *) right;
                rightC->cmresEval = context.code->makeNode<SimNode_GetLocalRefOff>(rE->at, stackTop, offset);
                return rightC;
            }
        }
        // now, invoke with CMRES
        if ( rE->rtti_isInvoke() ) {
            auto cll = static_pointer_cast<ExprInvoke>(rE);
            if ( cll->isCopyOrMove() ) {
                SimNode_CallBase * rightC = (SimNode_CallBase *) right;
                rightC->cmresEval = context.code->makeNode<SimNode_GetLocalRefOff>(rE->at, stackTop, offset);
                return rightC;
            }
        }
        // wo standard path
        auto left = context.code->makeNode<SimNode_GetLocalRefOff>(rE->at, stackTop, offset);
        if ( rightType.isHandle() ) {
            auto resN = rightType.annotation->simulateCopy(context, at, left,
                (!rightType.isRefType() && rightType.ref) ? rightType.annotation->simulateRef2Value(context, at, right) : right);
            if ( !resN ) {
                context.thisProgram->error("integration error, simulateCopy returned null", "", "",
                                           at, CompilationError::missing_node );
            }
            return resN;
        } else if ( rightType.isRef() ) {
            return context.code->makeNode<SimNode_CopyRefValue>(at, left, right, rightType.getSizeOf());
        } else {
            return context.code->makeValueNode<SimNode_Set>(rightType.baseType, at, left, right);
        }
    }

    SimNode * makeLocalMove (const LineInfo & at, Context & context, uint32_t stackTop, const ExpressionPtr & rE ) {
        const auto & rightType = *rE->type;
        // now, call with CMRES
        if ( rE->rtti_isCall() ) {
            auto cll = static_pointer_cast<ExprCall>(rE);
            if ( cll->func->copyOnReturn || cll->func->moveOnReturn ) {
                SimNode_CallBase * right = (SimNode_CallBase *) rE->simulate(context);
                right->cmresEval = context.code->makeNode<SimNode_GetLocal>(rE->at, stackTop);
                return right;
            }
        }
        // now, invoke with CMRES
        if ( rE->rtti_isInvoke() ) {
            auto cll = static_pointer_cast<ExprInvoke>(rE);
            if ( cll->isCopyOrMove() ) {
                SimNode_CallBase * right = (SimNode_CallBase *) rE->simulate(context);
                right->cmresEval = context.code->makeNode<SimNode_GetLocal>(rE->at, stackTop);
                return right;
            }
        }
        // now, to the regular move
        auto left = context.code->makeNode<SimNode_GetLocal>(at, stackTop);
        auto right = rE->simulate(context);
        if ( rightType.isRef() ) {
            return context.code->makeNode<SimNode_MoveRefValue>(at, left, right, rightType.getSizeOf());
        } else {
            DAS_ASSERTF(0, "we are calling makeLocalMove where expression on a right is not a referece."
                        "we should not be here, script compiler should have caught this during compilation."
                        "compiler later will likely report internal compilation error.");
            return nullptr;
        }
    }

    SimNode * makeLocalCopy(const LineInfo & at, Context & context, uint32_t stackTop, const ExpressionPtr & rE ) {
        const auto & rightType = *rE->type;
        assert ( rightType.canCopy() &&
                "we are calling makeLocalCopy on a type, which can't be copied."
                "we should not be here, script compiler should have caught this during compilation."
                "compiler later will likely report internal compilation error.");
        // now, call with CMRES
        if ( rE->rtti_isCall() ) {
            auto cll = static_pointer_cast<ExprCall>(rE);
            if ( cll->func->copyOnReturn || cll->func->moveOnReturn ) {
                SimNode_CallBase * right = (SimNode_CallBase *) rE->simulate(context);
                right->cmresEval = context.code->makeNode<SimNode_GetLocal>(rE->at, stackTop);
                return right;
            }
        }
        // now, invoke with CMRES
        if ( rE->rtti_isInvoke() ) {
            auto cll = static_pointer_cast<ExprInvoke>(rE);
            if ( cll->isCopyOrMove() ) {
                SimNode_CallBase * right = (SimNode_CallBase *) rE->simulate(context);
                right->cmresEval = context.code->makeNode<SimNode_GetLocal>(rE->at, stackTop);
                return right;
            }
        }
        // now, to the regular copy
        auto left = context.code->makeNode<SimNode_GetLocal>(rE->at, stackTop);
        auto right = rE->simulate(context);
        if ( rightType.isHandle() ) {
            auto resN = rightType.annotation->simulateCopy(context, at, left,
                (!rightType.isRefType() && rightType.ref) ? rightType.annotation->simulateRef2Value(context, at, right) : right);
            if ( !resN ) {
                context.thisProgram->error("integration error, simulateCopy returned null", "", "",
                                           at, CompilationError::missing_node );
            }
            return resN;
        } else if ( rightType.isRef() ) {
            return context.code->makeNode<SimNode_CopyRefValue>(at, left, right, rightType.getSizeOf());
        } else {
            return context.code->makeValueNode<SimNode_Set>(rightType.baseType, at, left, right);
        }
    }

    SimNode * makeCopy(const LineInfo & at, Context & context, const ExpressionPtr & lE, const ExpressionPtr & rE ) {
        const auto & rightType = *rE->type;
        assert ( rightType.canCopy() &&
                "we are calling makeCopy on a type, which can't be copied."
                "we should not be here, script compiler should have caught this during compilation."
                "compiler later will likely report internal compilation error.");
        // now, call with CMRES
        if ( rE->rtti_isCall() ) {
            auto cll = static_pointer_cast<ExprCall>(rE);
            if ( cll->func->copyOnReturn || cll->func->moveOnReturn ) {
                SimNode_CallBase * right = (SimNode_CallBase *) rE->simulate(context);
                right->cmresEval = lE->simulate(context);
                return right;
            }
        }
        // now, invoke with CMRES
        if ( rE->rtti_isInvoke() ) {
            auto cll = static_pointer_cast<ExprInvoke>(rE);
            if ( cll->isCopyOrMove() ) {
                SimNode_CallBase * right = (SimNode_CallBase *) rE->simulate(context);
                right->cmresEval = lE->simulate(context);
                return right;
            }
        }
        // now, to the regular copy
        auto left = lE->simulate(context);
        auto right = rE->simulate(context);
        if ( rightType.isHandle() ) {
            auto resN = rightType.annotation->simulateCopy(context, at, left,
                (!rightType.isRefType() && rightType.ref) ? rightType.annotation->simulateRef2Value(context, at, right) : right);
            if ( !resN ) {
                context.thisProgram->error("integration error, simulateCopy returned null", "", "",
                    at, CompilationError::missing_node );
            }
            return resN;
        } else if ( rightType.isRef() ) {
            return context.code->makeNode<SimNode_CopyRefValue>(at, left, right, rightType.getSizeOf());
        } else {
            return context.code->makeValueNode<SimNode_Set>(rightType.baseType, at, left, right);
        }
    }

    SimNode * makeMove (const LineInfo & at, Context & context, const ExpressionPtr & lE, const ExpressionPtr & rE ) {
        const auto & rightType = *rE->type;
        // now, call with CMRES
        if ( rE->rtti_isCall() ) {
            auto cll = static_pointer_cast<ExprCall>(rE);
            if ( cll->func->copyOnReturn || cll->func->moveOnReturn ) {
                SimNode_CallBase * right = (SimNode_CallBase *) rE->simulate(context);
                right->cmresEval = lE->simulate(context);
                return right;
            }
        }
        // now, invoke with CMRES
        if ( rE->rtti_isInvoke() ) {
            auto cll = static_pointer_cast<ExprInvoke>(rE);
            if ( cll->isCopyOrMove() ) {
                SimNode_CallBase * right = (SimNode_CallBase *) rE->simulate(context);
                right->cmresEval = lE->simulate(context);
                return right;
            }
        }
        // now to the regular one
        if ( rightType.isRef() ) {
            auto left = lE->simulate(context);
            auto right = rE->simulate(context);
            return context.code->makeNode<SimNode_MoveRefValue>(at, left, right, rightType.getSizeOf());
        } else {
            // this here might happen during initialization, by moving value types
            // like var t <- 5
            // its ok to generate simplified set here
            auto left = lE->simulate(context);
            auto right = rE->simulate(context);
            return context.code->makeValueNode<SimNode_Set>(rightType.baseType, at, left, right);
        }
    }

    SimNode * Function::makeSimNode ( Context & context, const vector<ExpressionPtr> & ) {
#if DAS_DEBUGGER
        if ( context.thisProgram->getDebugger() ) {
            if ( copyOnReturn || moveOnReturn ) {
                return context.code->makeNodeUnrollAny<SimNodeDebug_CallAndCopyOrMove>(int(arguments.size()), at);
            } else if ( fastCall ) {
                return context.code->makeNodeUnrollAny<SimNodeDebug_FastCall>(int(arguments.size()), at);
            } else {
                return context.code->makeNodeUnrollAny<SimNodeDebug_Call>(int(arguments.size()), at);
            }
        } else
#endif
        {
            if ( copyOnReturn || moveOnReturn ) {
                return context.code->makeNodeUnrollAny<SimNode_CallAndCopyOrMove>(int(arguments.size()), at);
            } else if ( fastCall ) {
                return context.code->makeNodeUnrollAny<SimNode_FastCall>(int(arguments.size()), at);
            } else {
                return context.code->makeNodeUnrollAny<SimNode_Call>(int(arguments.size()), at);
            }
        }
    }

    SimNode * Function::simulate (Context & context) const {
        if ( builtIn ) {
            DAS_ASSERTF(0, "can only simulate non built-in function");
            return nullptr;
        }
        if ( fastCall ) {
            assert(totalStackSize == sizeof(Prologue) && "function can't allocate stack");
            assert((result->isWorkhorseType() || result->isVoid()) && "fastcall can only return a workhorse type");
            assert(body->rtti_isBlock() && "function must contain a block");
            auto block = static_pointer_cast<ExprBlock>(body);
            assert(block->list.size()==1 && "fastcall is only one expr in a function body");
            if ( block->list.back()->rtti_isReturn() ) {
                assert(block->list.back()->rtti_isReturn() && "fastcall body expr is return");
                auto retE = static_pointer_cast<ExprReturn>(block->list.back());
                assert(retE->subexpr && "fastcall must return a value");
                return retE->subexpr->simulate(context);
            } else {
                return block->list.back()->simulate(context);
            }
        } else {
            return body->simulate(context);
        }
    }

    SimNode * Expression::trySimulate (Context &, uint32_t, Type) const {
        return nullptr;
    }

    void ExprMakeLocal::setRefSp ( bool ref, bool cmres, uint32_t sp, uint32_t off ) {
        useStackRef = ref;
        useCMRES = cmres;
        doesNotNeedSp = true;
        doesNotNeedInit = true;
        stackTop = sp;
        extraOffset = off;
    }

    vector<SimNode *> ExprMakeLocal::simulateLocal ( Context & /*context*/ ) const {
        return vector<SimNode *>();
    }

    // variant

    void ExprMakeVariant::setRefSp ( bool ref, bool cmres, uint32_t sp, uint32_t off ) {
        ExprMakeLocal::setRefSp(ref, cmres, sp, off);
        int stride = makeType->getStride();
        // we go through all fields, and if its [[ ]] field
        // we tell it to piggy-back on our current sp, with appropriate offset
        int index = 0;
        for ( const auto & decl : variants ) {
            auto fieldVariant = makeType->findArgumentIndex(decl->name);
            assert(fieldVariant!=-1 && "should have failed in type infer otherwise");
            auto fieldType = makeType->argTypes[fieldVariant];
            if ( decl->value->rtti_isMakeLocal() ) {
                auto fieldOffset = makeType->getVariantFieldOffset(fieldVariant);
                uint32_t offset =  extraOffset + index*stride + fieldOffset;
                auto mkl = static_pointer_cast<ExprMakeLocal>(decl->value);
                mkl->setRefSp(ref, cmres, sp, offset);
            } else if ( decl->value->rtti_isCall() ) {
                auto cll = static_pointer_cast<ExprCall>(decl->value);
                if ( cll->func->copyOnReturn || cll->func->moveOnReturn ) {
                    cll->doesNotNeedSp = true;
                }
            } else if ( decl->value->rtti_isInvoke() ) {
                auto cll = static_pointer_cast<ExprInvoke>(decl->value);
                if ( cll->isCopyOrMove() ) {
                    cll->doesNotNeedSp = true;
                }
            }
            index++;
        }
    }

    vector<SimNode *> ExprMakeVariant::simulateLocal (Context & context) const {
        vector<SimNode *> simlist;
        int index = 0;
        int stride = makeType->getStride();
        // init with 0 it its 'default' initialization
        if ( stride && variants.empty() ) {
            int bytes = stride;
            SimNode * init0;
            if ( useCMRES ) {
                if ( bytes <= 32 ) {
                    init0 = context.code->makeNodeUnrollNZ<SimNode_InitLocalCMResN>(bytes, at,extraOffset);
                } else {
                    init0 = context.code->makeNode<SimNode_InitLocalCMRes>(at,extraOffset,bytes);
                }
            } else if ( useStackRef ) {
                init0 = context.code->makeNode<SimNode_InitLocalRef>(at,stackTop,extraOffset,bytes);
            } else {
                init0 = context.code->makeNode<SimNode_InitLocal>(at,stackTop + extraOffset,bytes);
            }
            simlist.push_back(init0);
        }
        // now fields
        for ( const auto & decl : variants ) {
            auto fieldVariant = makeType->findArgumentIndex(decl->name);
            assert(fieldVariant!=-1 && "should have failed in type infer otherwise");
            // lets set variant index
            uint32_t voffset = extraOffset + index*stride;
            auto vconst = make_smart<ExprConstInt>(at, int32_t(fieldVariant));
            vconst->type = make_smart<TypeDecl>(Type::tInt);
            SimNode * svi;
            if ( useCMRES ) {
                svi = makeLocalCMResCopy(at,context,voffset,vconst);
            } else if (useStackRef) {
                svi = makeLocalRefCopy(at,context,stackTop,voffset,vconst);
            } else {
                svi = makeLocalCopy(at,context,stackTop+voffset,vconst);
            }
            simlist.push_back(svi);
            // field itself
            auto fieldOffset = makeType->getVariantFieldOffset(fieldVariant);
            uint32_t offset =  voffset + fieldOffset;
            SimNode * cpy;
            if ( decl->value->rtti_isMakeLocal() ) {
                // so what happens here, is we ask it for the generated commands and append it to this list only
                auto mkl = static_pointer_cast<ExprMakeLocal>(decl->value);
                auto lsim = mkl->simulateLocal(context);
                simlist.insert(simlist.end(), lsim.begin(), lsim.end());
                continue;
            } else if ( useCMRES ) {
                if ( decl->moveSemantics ){
                    cpy = makeLocalCMResMove(at,context,offset,decl->value);
                } else {
                    cpy = makeLocalCMResCopy(at,context,offset,decl->value);
                }
            } else if ( useStackRef ) {
                if ( decl->moveSemantics ){
                    cpy = makeLocalRefMove(at,context,stackTop,offset,decl->value);
                } else {
                    cpy = makeLocalRefCopy(at,context,stackTop,offset,decl->value);
                }
            } else {
                if ( decl->moveSemantics ){
                    cpy = makeLocalMove(at,context,stackTop+offset,decl->value);
                } else {
                    cpy = makeLocalCopy(at,context,stackTop+offset,decl->value);
                }
            }
            if ( !cpy ) {
                context.thisProgram->error("internal compilation error, can't generate structure initialization", "", "", at);
            }
            simlist.push_back(cpy);
            index++;
        }
        return simlist;
    }

    SimNode * ExprMakeVariant::simulate (Context & context) const {
        SimNode_Block * block;
        if ( useCMRES ) {
            block = context.code->makeNode<SimNode_MakeLocalCMRes>(at);
        } else {
            block = context.code->makeNode<SimNode_MakeLocal>(at, stackTop);
        }
        auto simlist = simulateLocal(context);
        block->total = int(simlist.size());
        block->list = (SimNode **) context.code->allocate(sizeof(SimNode *)*block->total);
        for ( uint32_t i = 0; i != block->total; ++i )
            block->list[i] = simlist[i];
        return block;
    }

    // structure

    void ExprMakeStruct::setRefSp ( bool ref, bool cmres, uint32_t sp, uint32_t off ) {
        ExprMakeLocal::setRefSp(ref, cmres, sp, off);
        // if it's a handle type, we can't reuse the make-local chain
        if ( makeType->baseType == Type::tHandle ) return;
        // we go through all fields, and if its [[ ]] field
        // we tell it to piggy-back on our current sp, with appropriate offset
        int total = int(structs.size());
        int stride = makeType->getStride();
        for ( int index=0; index != total; ++index ) {
            auto & fields = structs[index];
            for ( const auto & decl : *fields ) {
                auto field = makeType->structType->findField(decl->name);
                DAS_ASSERT(field && "should have failed in type infer otherwise");
                if ( decl->value->rtti_isMakeLocal() ) {
                    uint32_t offset =  extraOffset + index*stride + field->offset;
                    auto mkl = static_pointer_cast<ExprMakeLocal>(decl->value);
                    mkl->setRefSp(ref, cmres, sp, offset);
                } else if ( decl->value->rtti_isCall() ) {
                    auto cll = static_pointer_cast<ExprCall>(decl->value);
                    if ( cll->func->copyOnReturn || cll->func->moveOnReturn ) {
                        cll->doesNotNeedSp = true;
                    }
                } else if ( decl->value->rtti_isInvoke() ) {
                    auto cll = static_pointer_cast<ExprInvoke>(decl->value);
                    if ( cll->isCopyOrMove() ) {
                        cll->doesNotNeedSp = true;
                    }
                }
            }
        }
    }

    vector<SimNode *> ExprMakeStruct::simulateLocal (Context & context) const {
        vector<SimNode *> simlist;
        // init with 0
        int total = int(structs.size());
        int stride = makeType->getStride();
        if ( !doesNotNeedInit && !initAllFields && stride ) {
            int bytes = das::max(total,1) * stride;
            SimNode * init0;
            if ( useCMRES ) {
                if ( bytes <= 32 ) {
                    init0 = context.code->makeNodeUnrollNZ<SimNode_InitLocalCMResN>(bytes, at,extraOffset);
                } else {
                    init0 = context.code->makeNode<SimNode_InitLocalCMRes>(at,extraOffset,bytes);
                }
            } else if ( useStackRef ) {
                init0 = context.code->makeNode<SimNode_InitLocalRef>(at,stackTop,extraOffset,bytes);
            } else {
                init0 = context.code->makeNode<SimNode_InitLocal>(at,stackTop + extraOffset,bytes);
            }
            simlist.push_back(init0);
        }
        if ( makeType->baseType == Type::tStructure ) {
            for ( int index=0; index != total; ++index ) {
                auto & fields = structs[index];
                for ( const auto & decl : *fields ) {
                    auto field = makeType->structType->findField(decl->name);
                    assert(field && "should have failed in type infer otherwise");
                    uint32_t offset =  extraOffset + index*stride + field->offset;
                    SimNode * cpy;
                    if ( decl->value->rtti_isMakeLocal() ) {
                        // so what happens here, is we ask it for the generated commands and append it to this list only
                        auto mkl = static_pointer_cast<ExprMakeLocal>(decl->value);
                        auto lsim = mkl->simulateLocal(context);
                        simlist.insert(simlist.end(), lsim.begin(), lsim.end());
                        continue;
                    } else if ( useCMRES ) {
                        if ( decl->moveSemantics ){
                            cpy = makeLocalCMResMove(at,context,offset,decl->value);
                        } else {
                            cpy = makeLocalCMResCopy(at,context,offset,decl->value);
                        }
                    } else if ( useStackRef ) {
                        if ( decl->moveSemantics ){
                            cpy = makeLocalRefMove(at,context,stackTop,offset,decl->value);
                        } else {
                            cpy = makeLocalRefCopy(at,context,stackTop,offset,decl->value);
                        }
                    } else {
                        if ( decl->moveSemantics ){
                            cpy = makeLocalMove(at,context,stackTop+offset,decl->value);
                        } else {
                            cpy = makeLocalCopy(at,context,stackTop+offset,decl->value);
                        }
                    }
                    if ( !cpy ) {
                        context.thisProgram->error("internal compilation error, can't generate structure initialization", "", "", at);
                    }
                    simlist.push_back(cpy);
                }
            }
        } else {
            auto ann = makeType->annotation;
            // making fake variable, which points to out field
            string fakeName = "__makelocal";
            auto fakeVariable = make_smart<Variable>();
            fakeVariable->name = fakeName;
            fakeVariable->type = make_smart<TypeDecl>(Type::tHandle);
            fakeVariable->type->annotation = ann;
            fakeVariable->at = at;
            if ( useCMRES ) {
                fakeVariable->aliasCMRES = true;
            } else if ( useStackRef ) {
                fakeVariable->stackTop = stackTop + extraOffset;
                fakeVariable->type->ref = true;
                if ( total != 1 ) {
                    fakeVariable->type->dim.push_back(total);
                }
            }
            fakeVariable->generated = true;
            // make fake ExprVar which is that field
            auto fakeVar = make_smart<ExprVar>(at, fakeName);
            fakeVar->type = fakeVariable->type;
            fakeVar->variable = fakeVariable;
            fakeVar->local = true;
            // make fake expression
            ExpressionPtr fakeExpr = fakeVar;
            smart_ptr<ExprConstInt> indexExpr;
            if ( useStackRef && total > 1 ) {
                // if its stackRef with multiple indices, its actually var[total], and lookup is var[index]
                indexExpr = make_smart<ExprConstInt>(at, 0);
                indexExpr->type = make_smart<TypeDecl>(Type::tInt);
                fakeExpr = make_smart<ExprAt>(at, fakeExpr, indexExpr);
                fakeExpr->type = make_smart<TypeDecl>(Type::tHandle);
                fakeExpr->type->annotation = ann;
                fakeExpr->type->ref = true;
            }
            for ( int index=0; index != total; ++index ) {
                auto & fields = structs[index];
                // adjust var for index
                if ( useCMRES ) {
                    fakeVariable->stackTop = extraOffset + index*stride;
                } else if ( useStackRef ) {
                    if ( total > 1 ) {
                        indexExpr->value = cast<int32_t>::from(index);
                    }
                } else {
                    fakeVariable->stackTop = stackTop + extraOffset + index*stride;
                }
                // now, setup fields
                for ( const auto & decl : *fields ) {
                    auto fieldType = ann->makeFieldType(decl->name, false);
                    DAS_ASSERT(fieldType && "how did this infer?");
                    uint32_t fieldSize = fieldType->getSizeOf();
                    SimNode * cpy = nullptr;
                    auto left = ann->simulateGetField(decl->name, context, at, fakeExpr);
                    auto right = decl->value->simulate(context);
                    if ( !decl->value->type->isRef() ) {
                        if ( decl->value->type->isHandle() ) {
                            auto rightType = decl->value->type;
                            cpy = rightType->annotation->simulateCopy(context, at, left,
                                (!rightType->isRefType() && rightType->ref) ? rightType->annotation->simulateRef2Value(context, at, right) : right);
                            if ( !cpy ) {
                                context.thisProgram->error("integration error, simulateCopy returned null", "", "",
                                                        at, CompilationError::missing_node );
                            }
                        } else {
                            cpy = context.code->makeValueNode<SimNode_Set>(decl->value->type->baseType, decl->at, left, right);
                        }
                    } else if ( decl->moveSemantics ) {
                        cpy = context.code->makeNode<SimNode_MoveRefValue>(decl->at, left, right, fieldSize);
                    } else {
                        cpy = context.code->makeNode<SimNode_CopyRefValue>(decl->at, left, right, fieldSize);
                    }
                    simlist.push_back(cpy);
                }
            }
        }
        if ( block ) {
            /*
                TODO: optimize
                    there is no point in making fake invoke expression, we can replace 'self' with fake variable we've made
                    however this needs to happen during infer, and this needs to have different visitor,
                    so that stack is allocated properly in subexpressions etc
            */
            // making fake variable, which points to entire structure
            string fakeName = "__makelocal";
            auto fakeVariable = make_smart<Variable>();
            fakeVariable->name = fakeName;
            fakeVariable->type = make_smart<TypeDecl>(*type);
            if ( useCMRES ) {
                fakeVariable->aliasCMRES = true;
            } else if ( useStackRef ) {
                fakeVariable->stackTop = stackTop + extraOffset;
                fakeVariable->type->ref = true;
            } else {
                fakeVariable->stackTop = stackTop + extraOffset;
            }
            fakeVariable->generated = true;
            // make fake ExprVar which is that field
            auto fakeVar = make_smart<ExprVar>(at, fakeName);
            fakeVar->type = fakeVariable->type;
            fakeVar->variable = fakeVariable;
            fakeVar->local = true;
            // make fake invoke expression
            auto fakeInvoke = make_smart<ExprInvoke>(at,"invoke");
            fakeInvoke->arguments.push_back(block);
            fakeInvoke->arguments.push_back(fakeVar);
            // simulate it
            auto simI = fakeInvoke->simulate(context);
            simlist.push_back(simI);
        }
        return simlist;
    }

    SimNode * ExprMakeStruct::simulate (Context & context) const {
        SimNode_Block * blk;
        if ( useCMRES ) {
            blk = context.code->makeNode<SimNode_MakeLocalCMRes>(at);
        } else {
            blk = context.code->makeNode<SimNode_MakeLocal>(at, stackTop);
        }
        auto simlist = simulateLocal(context);
        blk->total = int(simlist.size());
        blk->list = (SimNode **) context.code->allocate(sizeof(SimNode *)*blk->total);
        for ( uint32_t i = 0; i != blk->total; ++i )
            blk->list[i] = simlist[i];
        return blk;
    }

    // make array

    void ExprMakeArray::setRefSp ( bool ref, bool cmres, uint32_t sp, uint32_t off ) {
        ExprMakeLocal::setRefSp(ref, cmres, sp, off);
        int total = int(values.size());
        uint32_t stride = recordType->getSizeOf();
        for ( int index=0; index != total; ++index ) {
            auto & val = values[index];
            if ( val->rtti_isMakeLocal() ) {
                uint32_t offset =  extraOffset + index*stride;
                auto mkl = static_pointer_cast<ExprMakeLocal>(val);
                mkl->setRefSp(ref, cmres, sp, offset);
            } else if ( val->rtti_isCall() ) {
                auto cll = static_pointer_cast<ExprCall>(val);
                if ( cll->func->copyOnReturn || cll->func->moveOnReturn ) {
                    cll->doesNotNeedSp = true;
                }
            } else if ( val->rtti_isInvoke() ) {
                auto cll = static_pointer_cast<ExprInvoke>(val);
                if ( cll->isCopyOrMove() ) {
                    cll->doesNotNeedSp = true;
                }
            }
        }
    }

    vector<SimNode *> ExprMakeArray::simulateLocal (Context & context) const {
        vector<SimNode *> simlist;
        // init with 0
        int total = int(values.size());
        uint32_t stride = recordType->getSizeOf();
        if ( !doesNotNeedInit && !initAllFields ) {
            int bytes = total * stride;
            SimNode * init0;
            if ( useCMRES ) {
                if ( bytes <= 32 ) {
                    init0 = context.code->makeNodeUnrollNZ<SimNode_InitLocalCMResN>(bytes, at,extraOffset);
                } else {
                    init0 = context.code->makeNode<SimNode_InitLocalCMRes>(at,extraOffset,bytes);
                }
            } else if ( useStackRef ) {
                init0 = context.code->makeNode<SimNode_InitLocalRef>(at,stackTop,extraOffset,stride * total);
            } else {
                init0 = context.code->makeNode<SimNode_InitLocal>(at,stackTop + extraOffset,stride * total);
            }
            simlist.push_back(init0);
        }
        for ( int index=0; index != total; ++index ) {
            auto & val = values[index];
            uint32_t offset = extraOffset + index*stride;
            SimNode * cpy;
            if ( val->rtti_isMakeLocal() ) {
                // so what happens here, is we ask it for the generated commands and append it to this list only
                auto mkl = static_pointer_cast<ExprMakeLocal>(val);
                auto lsim = mkl->simulateLocal(context);
                simlist.insert(simlist.end(), lsim.begin(), lsim.end());
                continue;
            } else if ( useCMRES ) {
                if (val->type->canCopy()) {
                    cpy = makeLocalCMResCopy(at, context, offset, val);
                } else {
                    cpy = makeLocalCMResMove(at, context, offset, val);
                }
            } else if ( useStackRef ) {
                if (val->type->canCopy()) {
                    cpy = makeLocalRefCopy(at, context, stackTop, offset, val);
                } else {
                    cpy = makeLocalRefMove(at, context, stackTop, offset, val);
                }
            } else {
                if (val->type->canCopy()) {
                    cpy = makeLocalCopy(at, context, stackTop + offset, val);
                } else {
                    cpy = makeLocalMove(at, context, stackTop + offset, val);
                }
            }
            if ( !cpy ) {
                context.thisProgram->error("internal compilation error, can't generate array initialization", "", "", at);
            }
            simlist.push_back(cpy);
        }
        return simlist;
    }

    SimNode * ExprMakeArray::simulate (Context & context) const {
        SimNode_Block * block;
        if ( useCMRES ) {
            block = context.code->makeNode<SimNode_MakeLocalCMRes>(at);
        } else {
            block = context.code->makeNode<SimNode_MakeLocal>(at, stackTop);
        }
        auto simlist = simulateLocal(context);
        block->total = int(simlist.size());
        block->list = (SimNode **) context.code->allocate(sizeof(SimNode *)*block->total);
        for ( uint32_t i = 0; i != block->total; ++i )
            block->list[i] = simlist[i];
        return block;
    }

    // make tuple

    void ExprMakeTuple::setRefSp ( bool ref, bool cmres, uint32_t sp, uint32_t off ) {
        ExprMakeLocal::setRefSp(ref, cmres, sp, off);
        int total = int(values.size());
        for ( int index=0; index != total; ++index ) {
            auto & val = values[index];
            if ( val->rtti_isMakeLocal() ) {
                uint32_t offset =  extraOffset + makeType->getTupleFieldOffset(index);
                auto mkl = static_pointer_cast<ExprMakeLocal>(val);
                mkl->setRefSp(ref, cmres, sp, offset);
            } else if ( val->rtti_isCall() ) {
                auto cll = static_pointer_cast<ExprCall>(val);
                if ( cll->func->copyOnReturn || cll->func->moveOnReturn ) {
                    cll->doesNotNeedSp = true;
                }
            } else if ( val->rtti_isInvoke() ) {
                auto cll = static_pointer_cast<ExprInvoke>(val);
                if ( cll->isCopyOrMove() ) {
                    cll->doesNotNeedSp = true;
                }
            }
        }
    }

    vector<SimNode *> ExprMakeTuple::simulateLocal (Context & context) const {
        vector<SimNode *> simlist;
        // init with 0
        int total = int(values.size());
        if ( !doesNotNeedInit && !initAllFields ) {
            uint32_t sizeOf = makeType->getSizeOf();
            SimNode * init0;
            if ( useCMRES ) {
                if ( sizeOf <= 32 ) {
                    init0 = context.code->makeNodeUnrollNZ<SimNode_InitLocalCMResN>(sizeOf, at,extraOffset);
                } else {
                    init0 = context.code->makeNode<SimNode_InitLocalCMRes>(at,extraOffset,sizeOf);
                }
            } else if ( useStackRef ) {
                init0 = context.code->makeNode<SimNode_InitLocalRef>(at,stackTop,extraOffset,sizeOf);
            } else {
                init0 = context.code->makeNode<SimNode_InitLocal>(at,stackTop + extraOffset,sizeOf);
            }
            simlist.push_back(init0);
        }
        for ( int index=0; index != total; ++index ) {
            auto & val = values[index];
            uint32_t offset = extraOffset + makeType->getTupleFieldOffset(index);
            SimNode * cpy;
            if ( val->rtti_isMakeLocal() ) {
                // so what happens here, is we ask it for the generated commands and append it to this list only
                auto mkl = static_pointer_cast<ExprMakeLocal>(val);
                auto lsim = mkl->simulateLocal(context);
                simlist.insert(simlist.end(), lsim.begin(), lsim.end());
                continue;
            } else if ( useCMRES ) {
                if (val->type->canCopy()) {
                    cpy = makeLocalCMResCopy(at, context, offset, val);
                } else {
                    cpy = makeLocalCMResMove(at, context, offset, val);
                }
            } else if ( useStackRef ) {
                if (val->type->canCopy()) {
                    cpy = makeLocalRefCopy(at, context, stackTop, offset, val);
                } else {
                    cpy = makeLocalRefMove(at, context, stackTop, offset, val);
                }
            } else {
                if (val->type->canCopy()) {
                    cpy = makeLocalCopy(at, context, stackTop + offset, val);
                } else {
                    cpy = makeLocalMove(at, context, stackTop + offset, val);
                }
            }
            if ( !cpy ) {
                context.thisProgram->error("internal compilation error, can't generate array initialization", "", "", at);
            }
            simlist.push_back(cpy);
        }
        return simlist;
    }


    SimNode * ExprMakeTuple::simulate (Context & context) const {
        SimNode_Block * block;
        if ( useCMRES ) {
            block = context.code->makeNode<SimNode_MakeLocalCMRes>(at);
        } else {
            block = context.code->makeNode<SimNode_MakeLocal>(at, stackTop);
        }
        auto simlist = simulateLocal(context);
        block->total = int(simlist.size());
        block->list = (SimNode **) context.code->allocate(sizeof(SimNode *)*block->total);
        for ( uint32_t i = 0; i != block->total; ++i )
            block->list[i] = simlist[i];
        return block;
    }

    // reader

    SimNode * ExprReader::simulate (Context & context) const {
        context.thisProgram->error("internal compilation error, calling 'simulate' on reader", "", "", at);
        return nullptr;
    }

    // label

    SimNode * ExprLabel::simulate (Context & context) const {
        context.thisProgram->error("internal compilation error, calling 'simulate' on label", "", "", at);
        return nullptr;
    }

    // goto

    SimNode * ExprGoto::simulate (Context & context) const {
        if ( subexpr ) {
            return context.code->makeNode<SimNode_Goto>(at, subexpr->simulate(context));
        } else {
            return context.code->makeNode<SimNode_GotoLabel>(at,label);
        }
    }

    // r2v

    SimNode * ExprRef2Value::GetR2V ( Context & context, const LineInfo & at, const TypeDeclPtr & type, SimNode * expr ) {
        if ( type->isHandle() ) {
            auto resN = type->annotation->simulateRef2Value(context, at, expr);
            if ( !resN ) {
                context.thisProgram->error("integration error, simulateRef2Value returned null", "", "",
                                           at, CompilationError::missing_node );
            }
            return resN;
        } else {
            if ( type->isRefType() ) {
                return expr;
            } else {
                return context.code->makeValueNode<SimNode_Ref2Value>(type->baseType, at, expr);
            }
        }
    }

    SimNode * ExprRef2Value::simulate (Context & context) const {
        return GetR2V(context, at, type, subexpr->simulate(context));
    }

    SimNode * ExprAddr::simulate (Context & context) const {
        assert(func->index>=0 && "how, we specified in the unused");
        Func fn; fn.index = func->index + 1;
        vec4f cval = v_zero();
        memcpy (&cval, &fn, sizeof(Func));
        return context.code->makeNode<SimNode_FuncConstValue>(at,cval);
    }

    SimNode * ExprPtr2Ref::simulate (Context & context) const {
        if ( unsafeDeref ) {
            return subexpr->simulate(context);
        } else {
            return context.code->makeNode<SimNode_Ptr2Ref>(at,subexpr->simulate(context));
        }
    }

    SimNode * ExprRef2Ptr::simulate (Context & context) const {
        return subexpr->simulate(context);
    }

    SimNode * ExprNullCoalescing::simulate (Context & context) const {
        if ( type->isRef() ) {
            return context.code->makeNode<SimNode_NullCoalescingRef>(at,subexpr->simulate(context),defaultValue->simulate(context));
        } else {
            return context.code->makeValueNode<SimNode_NullCoalescing>(type->baseType,at,subexpr->simulate(context),defaultValue->simulate(context));
        }
    }

    SimNode * ExprConst::simulate (Context & context) const {
        return context.code->makeNode<SimNode_ConstValue>(at,value);
    }

    SimNode * ExprConstEnumeration::simulate (Context & context) const {
        return context.code->makeNode<SimNode_ConstValue>(at, value);
    }

    SimNode * ExprConstString::simulate (Context & context) const {
        if ( !text.empty() ) {
            char* str = context.constStringHeap->allocateString(text);
            return context.code->makeNode<SimNode_ConstString>(at, str);
        } else {
            return context.code->makeNode<SimNode_ConstString>(at, nullptr);
        }
    }

    SimNode * ExprStaticAssert::simulate (Context &) const {
        return nullptr;
    }

    SimNode * ExprAssert::simulate (Context & context) const {
        string message;
        if ( arguments.size()==2 && arguments[1]->rtti_isStringConstant() )
            message = static_pointer_cast<ExprConstString>(arguments[1])->getValue();
        return context.code->makeNode<SimNode_Assert>(at,arguments[0]->simulate(context),context.constStringHeap->allocateString(message));
    }

    struct SimNode_AstGetExpression : SimNode_CallBase {
        DAS_PTR_NODE;
        SimNode_AstGetExpression ( const LineInfo & at, const ExpressionPtr & e, char * d )
            : SimNode_CallBase(at) {
            expr = e.get();
            descr = d;
        }
        virtual SimNode * copyNode ( Context & context, NodeAllocator * code ) override {
            auto that = (SimNode_AstGetExpression *) SimNode::copyNode(context, code);
            that->descr = code->allocateName(descr);
            return that;
        }
        virtual SimNode * visit ( SimVisitor & vis ) override {
            V_BEGIN();
            V_OP(AstGetExpression);
            V_ARG(descr);
            V_END();
        }
        __forceinline char * compute(Context &) {
            DAS_PROFILE_NODE
            return (char *) expr->clone().orphan();
        }
        Expression *  expr;   // requires RTTI
        char *        descr;
    };

    SimNode * ExprQuote::simulate (Context & context) const {
        DAS_ASSERTF(arguments.size()==1,"Quote expects to return only one ExpressionPtr."
        "We should not be here, since typeinfer should catch the mismatch.");
        TextWriter ss;
        ss << *arguments[0];
        char * descr = context.code->allocateName(ss.str());
        return context.code->makeNode<SimNode_AstGetExpression>(at, arguments[0], descr);
    }

    SimNode * ExprDebug::simulate (Context & context) const {
        TypeInfo * pTypeInfo = context.thisHelper->makeTypeInfo(nullptr, arguments[0]->type);
        string message;
        if ( arguments.size()==2 && arguments[1]->rtti_isStringConstant() )
            message = static_pointer_cast<ExprConstString>(arguments[1])->getValue();
        return context.code->makeNode<SimNode_Debug>(at,
                                               arguments[0]->simulate(context),
                                               pTypeInfo,
                                               context.constStringHeap->allocateString(message));
    }

    SimNode * ExprMemZero::simulate (Context & context) const {
        const auto & subexpr = arguments[0];
        uint32_t dataSize = subexpr->type->getSizeOf();
        return context.code->makeNode<SimNode_MemZero>(at, subexpr->simulate(context), dataSize);
    }

    SimNode * ExprMakeGenerator::simulate (Context & context) const {
        DAS_ASSERTF(0, "we should not be here ever, ExprMakeGenerator should completly fold during type inference.");
        context.thisProgram->error("internal compilation error, generating node for ExprMakeGenerator", "", "", at);
        return nullptr;
    }

    SimNode * ExprYield::simulate (Context & context) const {
        DAS_ASSERTF(0, "we should not be here ever, ExprYield should completly fold during type inference.");
        context.thisProgram->error("internal compilation error, generating node for ExprYield", "", "", at);
        return nullptr;
    }

    SimNode * ExprArrayComprehension::simulate (Context & context) const {
        DAS_ASSERTF(0, "we should not be here ever, ExprArrayComprehension should completly fold during type inference.");
        context.thisProgram->error("internal compilation error, generating node for ExprArrayComprehension", "", "", at);
        return nullptr;
    }

    SimNode * ExprMakeBlock::simulate (Context & context) const {
        auto blk = static_pointer_cast<ExprBlock>(block);
        uint32_t argSp = blk->stackTop;
        auto info = context.thisHelper->makeInvokeableTypeDebugInfo(blk->makeBlockType(),blk->at);
        if ( context.thisProgram->getDebugger() ) {
            context.thisHelper->appendLocalVariables(info, (Expression *)this);
        }
        return context.code->makeNode<SimNode_MakeBlock>(at,block->simulate(context),argSp,stackTop,info);
    }

    bool ExprInvoke::isCopyOrMove() const {
        auto blockT = arguments[0]->type;
        return blockT->firstType && blockT->firstType->isRefType() && !blockT->firstType->ref;
    }

    SimNode * ExprInvoke::simulate (Context & context) const {
        auto blockT = arguments[0]->type;
        SimNode_CallBase * pInvoke;
#if DAS_DEBUGGER
        if ( context.thisProgram->getDebugger() ) {
            if ( isCopyOrMove() ) {
                auto getSp = context.code->makeNode<SimNode_GetLocal>(at,stackTop);
                if ( blockT->baseType==Type::tBlock ) {
                    pInvoke = (SimNode_CallBase *) context.code->makeNodeUnrollAny<SimNodeDebug_InvokeAndCopyOrMove>(
                                                        int(arguments.size()), at, getSp);
                } else if ( blockT->baseType==Type::tFunction ) {
                    pInvoke = (SimNode_CallBase *) context.code->makeNodeUnrollAny<SimNodeDebug_InvokeAndCopyOrMoveFn>(
                                                        int(arguments.size()), at, getSp);
                } else {
                    pInvoke = (SimNode_CallBase *) context.code->makeNodeUnrollAny<SimNodeDebug_InvokeAndCopyOrMoveLambda>(
                                                        int(arguments.size()), at, getSp);
                }
            } else {
                if ( blockT->baseType==Type::tBlock ) {
                    pInvoke = (SimNode_CallBase *) context.code->makeNodeUnrollAny<SimNodeDebug_Invoke>(int(arguments.size()),at);
                } else if ( blockT->baseType==Type::tFunction ) {
                    pInvoke = (SimNode_CallBase *) context.code->makeNodeUnrollAny<SimNodeDebug_InvokeFn>(int(arguments.size()),at);
                } else {
                    pInvoke = (SimNode_CallBase *) context.code->makeNodeUnrollAny<SimNodeDebug_InvokeLambda>(int(arguments.size()),at);
                }
            }
        } else
#endif
        {
            if ( isCopyOrMove() ) {
                auto getSp = context.code->makeNode<SimNode_GetLocal>(at,stackTop);
                if ( blockT->baseType==Type::tBlock ) {
                    pInvoke = (SimNode_CallBase *) context.code->makeNodeUnrollAny<SimNode_InvokeAndCopyOrMove>(
                                                        int(arguments.size()), at, getSp);
                } else if ( blockT->baseType==Type::tFunction ) {
                    pInvoke = (SimNode_CallBase *) context.code->makeNodeUnrollAny<SimNode_InvokeAndCopyOrMoveFn>(
                                                        int(arguments.size()), at, getSp);
                } else {
                    pInvoke = (SimNode_CallBase *) context.code->makeNodeUnrollAny<SimNode_InvokeAndCopyOrMoveLambda>(
                                                        int(arguments.size()), at, getSp);
                }
            } else {
                if ( blockT->baseType==Type::tBlock ) {
                    pInvoke = (SimNode_CallBase *) context.code->makeNodeUnrollAny<SimNode_Invoke>(int(arguments.size()),at);
                } else if ( blockT->baseType==Type::tFunction ) {
                    pInvoke = (SimNode_CallBase *) context.code->makeNodeUnrollAny<SimNode_InvokeFn>(int(arguments.size()),at);
                } else {
                    pInvoke = (SimNode_CallBase *) context.code->makeNodeUnrollAny<SimNode_InvokeLambda>(int(arguments.size()),at);
                }
            }
        }
        pInvoke->debugInfo = at;
        if ( int nArg = (int) arguments.size() ) {
            pInvoke->arguments = (SimNode **) context.code->allocate(nArg * sizeof(SimNode *));
            pInvoke->nArguments = nArg;
            for ( int a=0; a!=nArg; ++a ) {
                pInvoke->arguments[a] = arguments[a]->simulate(context);
            }
        } else {
            pInvoke->arguments = nullptr;
            pInvoke->nArguments = 0;
        }
        return pInvoke;
    }

    SimNode * ExprErase::simulate (Context & context) const {
        auto cont = arguments[0]->simulate(context);
        auto val = arguments[1]->simulate(context);
        if ( arguments[0]->type->isGoodTableType() ) {
            uint32_t valueTypeSize = arguments[0]->type->secondType->getSizeOf();
            return context.code->makeValueNode<SimNode_TableErase>(arguments[0]->type->firstType->baseType, at, cont, val, valueTypeSize);
        } else {
            DAS_ASSERTF(0, "we should not even be here. erase can only accept tables. infer type should have failed.");
            context.thisProgram->error("internal compilation error, generating erase for non-table type", "", "", at);
            return nullptr;
        }
    }

    SimNode * ExprFind::simulate (Context & context) const {
        auto cont = arguments[0]->simulate(context);
        auto val = arguments[1]->simulate(context);
        if ( arguments[0]->type->isGoodTableType() ) {
            uint32_t valueTypeSize = arguments[0]->type->secondType->getSizeOf();
            return context.code->makeValueNode<SimNode_TableFind>(arguments[0]->type->firstType->baseType, at, cont, val, valueTypeSize);
        } else {
            DAS_ASSERTF(0, "we should not even be here. find can only accept tables. infer type should have failed.");
            context.thisProgram->error("internal compilation error, generating find for non-table type", "", "", at);
            return nullptr;
        }
    }

    SimNode * ExprKeyExists::simulate (Context & context) const {
        auto cont = arguments[0]->simulate(context);
        auto val = arguments[1]->simulate(context);
        if ( arguments[0]->type->isGoodTableType() ) {
            uint32_t valueTypeSize = arguments[0]->type->secondType->getSizeOf();
            return context.code->makeValueNode<SimNode_KeyExists>(arguments[0]->type->firstType->baseType, at, cont, val, valueTypeSize);
        } else {
            DAS_ASSERTF(0, "we should not even be here. find can only accept tables. infer type should have failed.");
            context.thisProgram->error("internal compilation error, generating find for non-table type", "", "", at);
            return nullptr;
        }
    }

    SimNode * ExprIs::simulate (Context & context) const {
        DAS_ASSERTF(0, "we should not even be here. 'is' should resolve to const during infer pass.");
        context.thisProgram->error("internal compilation error, generating 'is'", "", "", at);
        return nullptr;
    }

    SimNode * ExprTypeInfo::simulate (Context & context) const {
        if ( !macro ) {
            DAS_ASSERTF(0, "we should not even be here. typeinfo should resolve to const during infer pass.");
            context.thisProgram->error("internal compilation error, generating typeinfo(...)", "", "", at);
            return nullptr;
        } else {
            string errors;
            auto node = macro->simluate(&context, (Expression*)this, errors);
            if ( !node || !errors.empty() ) {
                context.thisProgram->error("typeinfo(" + trait + "...) macro generated no node; " + errors,
                    "", "", at, CompilationError::typeinfo_macro_error);
            }
            return node;
        }
    }

    SimNode * ExprDelete::simulate (Context & context) const {
        uint32_t total = uint32_t(subexpr->type->getCountOf());
        DAS_ASSERTF(total==1,"we should not be deleting more than one at a time");
        auto sube = subexpr->simulate(context);
        if ( subexpr->type->baseType==Type::tArray ) {
            auto stride = subexpr->type->firstType->getSizeOf();
            return context.code->makeNode<SimNode_DeleteArray>(at, sube, total, stride);
        } else if ( subexpr->type->baseType==Type::tTable ) {
            auto vts_add_kts = subexpr->type->firstType->getSizeOf() +
                subexpr->type->secondType->getSizeOf();
            return context.code->makeNode<SimNode_DeleteTable>(at, sube, total, vts_add_kts);
        } else if ( subexpr->type->baseType==Type::tPointer ) {
            if ( subexpr->type->firstType->baseType==Type::tStructure ) {
                auto structSize = subexpr->type->firstType->getSizeOf();
                bool persistent = subexpr->type->firstType->structType->persistent;
                bool isLambda = subexpr->type->firstType->structType->isLambda;
                return context.code->makeNode<SimNode_DeleteStructPtr>(at, sube, total, structSize, persistent, isLambda);
            } else if ( subexpr->type->firstType->baseType==Type::tTuple ) {
                auto structSize = subexpr->type->firstType->getSizeOf();
                return context.code->makeNode<SimNode_DeleteStructPtr>(at, sube, total, structSize, false, false);
            } else if ( subexpr->type->firstType->baseType==Type::tVariant ) {
                auto structSize = subexpr->type->firstType->getSizeOf();
                return context.code->makeNode<SimNode_DeleteStructPtr>(at, sube, total, structSize, false, false);
            } else {
                auto ann = subexpr->type->firstType->annotation;
                assert(ann->canDeletePtr() && "has to be able to delete ptr");
                auto resN = ann->simulateDeletePtr(context, at, sube, total);
                if ( !resN ) {
                    context.thisProgram->error("integration error, simulateDelete returned null", "", "",
                                               at, CompilationError::missing_node );
                }
                return resN;
            }
        } else if ( subexpr->type->baseType==Type::tHandle ) {
            auto ann = subexpr->type->annotation;
            assert(ann->canDelete() && "has to be able to delete");
            auto resN =  ann->simulateDelete(context, at, sube, total);
            if ( !resN ) {
                context.thisProgram->error("integration error, simulateDelete returned null", "", "",
                                           at, CompilationError::missing_node );
            }
            return resN;
        } else if ( subexpr->type->baseType==Type::tLambda ) {
            return context.code->makeNode<SimNode_DeleteLambda>(at, sube, total);
        } else {
            DAS_ASSERTF(0, "we should not be here. this is delete for unsupported type. infer types should have failed.");
            context.thisProgram->error("internal compilation error, generating node for unsupported ExprDelete", "", "", at);
            return nullptr;
        }
    }
    SimNode * ExprCast::trySimulate (Context & context, uint32_t extraOffset, Type r2vType ) const {
        return subexpr->trySimulate(context, extraOffset, r2vType);
    }

    SimNode * ExprCast::simulate (Context & context) const {
        return subexpr->simulate(context);
    }

    SimNode * ExprAscend::simulate (Context & context) const {
        auto se = subexpr->simulate(context);
        auto bytes = subexpr->type->getSizeOf();
        TypeInfo * typeInfo = nullptr;
        if ( needTypeInfo ) {
            typeInfo = context.thisHelper->makeTypeInfo(nullptr, subexpr->type);
        }
        if ( subexpr->type->baseType==Type::tHandle ) {
            DAS_ASSERTF(useStackRef,"new of handled type should always be over stackref");
            auto ne = subexpr->type->annotation->simulateGetNew(context, at);
            return context.code->makeNode<SimNode_AscendNewHandleAndRef>(at, se, ne, bytes, stackTop);
        } else {
            bool peristent = false;
            if ( subexpr->type->baseType==Type::tStructure ) {
                peristent = subexpr->type->structType->persistent;
            }
            if ( useStackRef ) {
                return context.code->makeNode<SimNode_AscendAndRef>(at, se, bytes, stackTop, typeInfo, peristent);
            } else {
                return context.code->makeNode<SimNode_Ascend<false>>(at, se, bytes, typeInfo, peristent);
            }
        }
    }

    SimNode * ExprNew::simulate (Context & context) const {
        SimNode * newNode;
        if ( typeexpr->baseType == Type::tHandle ) {
            assert(typeexpr->annotation->canNew() && "how???");
            newNode = typeexpr->annotation->simulateGetNew(context, at);
            if ( !newNode ) {
                context.thisProgram->error("integration error, simulateGetNew returned null", "", "",
                                           at, CompilationError::missing_node );
            }
        } else {
            bool persistent = false;
            if ( typeexpr->baseType == Type::tStructure ) {
                persistent = typeexpr->structType->persistent;
            }
            int32_t bytes = type->firstType->getSizeOf();
            if ( initializer ) {
                auto pCall = (SimNode_CallBase *) context.code->makeNodeUnrollAny<SimNode_NewWithInitializer>(
                    int(arguments.size()),at,bytes,persistent);
                pCall->cmresEval = nullptr;
                newNode = ExprCall::simulateCall(func, this, context, pCall);
            } else {
                newNode = context.code->makeNode<SimNode_New>(at,bytes,persistent);
            }
        }
        if ( type->dim.size() ) {
            uint32_t count = type->getCountOf();
            return context.code->makeNode<SimNode_NewArray>(at,newNode,stackTop,count);
        } else {
            return newNode;
        }
    }

    SimNode * ExprAt::trySimulate (Context & context, uint32_t extraOffset, Type r2vType ) const {
        if ( subexpr->type->isVectorType() ) {
            return nullptr;
        } else if ( subexpr->type->isGoodTableType() ) {
            return nullptr;
        } else if ( subexpr->type->isHandle() ) {
            SimNode * result;
            if ( r2vType!=Type::none ) {
                result = subexpr->type->annotation->simulateGetAtR2V(context, at, index->type, subexpr, index, extraOffset);
                if ( !result ) {
                    context.thisProgram->error("integration error, simulateGetAtR2V returned null", "", "",
                                               at, CompilationError::missing_node );
                }
            } else {
                result = subexpr->type->annotation->simulateGetAt(context, at, index->type, subexpr, index, extraOffset);
                if ( !result ) {
                    context.thisProgram->error("integration error, simulateGetAt returned null", "", "",
                                               at, CompilationError::missing_node );
                }
            }
            return result;
        } else if ( subexpr->type->isGoodArrayType() ) {
            auto prv = subexpr->simulate(context);
            auto pidx = index->simulate(context);
            uint32_t stride = subexpr->type->firstType->getSizeOf();
            if ( r2vType!=Type::none ) {
                return context.code->makeValueNode<SimNode_ArrayAtR2V>(r2vType, at, prv, pidx, stride, extraOffset);
            } else {
                return context.code->makeNode<SimNode_ArrayAt>(at, prv, pidx, stride, extraOffset);
            }
        } else if ( subexpr->type->isPointer() ) {
            uint32_t range = 0xffffffff;
            uint32_t stride = subexpr->type->firstType->getSizeOf();
            auto prv = subexpr->simulate(context);
            auto pidx = index->simulate(context);
            if ( r2vType!=Type::none ) {
                return context.code->makeValueNode<SimNode_AtR2V>(r2vType, at, prv, pidx, stride, extraOffset, range);
            } else {
                return context.code->makeNode<SimNode_At>(at, prv, pidx, stride, extraOffset, range);
            }
        } else {
            uint32_t range = subexpr->type->dim[0];
            uint32_t stride = subexpr->type->getStride();
            if ( index->rtti_isConstant() ) {
                // if its constant index, like a[3]..., we try to let node bellow simulate
                auto idxCE = static_pointer_cast<ExprConst>(index);
                uint32_t idxC = cast<uint32_t>::to(idxCE->value);
                if ( idxC >= range ) {
                    context.thisProgram->error("index out of range", "", "",
                        at, CompilationError::index_out_of_range);
                    return nullptr;
                }
                auto tnode = subexpr->trySimulate(context, extraOffset + idxC*stride, r2vType);
                if ( tnode ) {
                    return tnode;
                }
            }
            // regular scenario
            auto prv = subexpr->simulate(context);
            auto pidx = index->simulate(context);
            if ( r2vType!=Type::none ) {
                return context.code->makeValueNode<SimNode_AtR2V>(r2vType, at, prv, pidx, stride, extraOffset, range);
            } else {
                return context.code->makeNode<SimNode_At>(at, prv, pidx, stride, extraOffset, range);
            }
        }
    }

    SimNode * ExprAt::simulate (Context & context) const {
        if ( subexpr->type->isVectorType() ) {
            auto prv = subexpr->simulate(context);
            auto pidx = index->simulate(context);
            uint32_t range = subexpr->type->getVectorDim();
            uint32_t stride = type->getSizeOf();
            if ( subexpr->type->ref ) {
                auto res = context.code->makeNode<SimNode_At>(at, prv, pidx, stride, 0, range);
                if ( r2v ) {
                    return ExprRef2Value::GetR2V(context, at, type, res);
                } else {
                    return res;
                }
            } else {
                switch ( type->baseType ) {
                    case tInt:      return context.code->makeNode<SimNode_AtVector<int32_t>>(at, prv, pidx, range);
                    case tUInt:
                    case tBitfield:
                                    return context.code->makeNode<SimNode_AtVector<uint32_t>>(at, prv, pidx, range);
                    case tFloat:    return context.code->makeNode<SimNode_AtVector<float>>(at, prv, pidx, range);
                    default:
                        DAS_ASSERTF(0, "we should not even be here. infer type should have failed on unsupported_vector[blah]");
                        context.thisProgram->error("internal compilation error, generating vector at for unsupported vector type.", "", "", at);
                        return nullptr;
                }
            }
        } else if ( subexpr->type->isGoodTableType() ) {
            auto prv = subexpr->simulate(context);
            auto pidx = index->simulate(context);
            uint32_t valueTypeSize = subexpr->type->secondType->getSizeOf();
            auto res = context.code->makeValueNode<SimNode_TableIndex>(subexpr->type->firstType->baseType, at, prv, pidx, valueTypeSize, 0);
            if ( r2v ) {
                return ExprRef2Value::GetR2V(context, at, type, res);
            } else {
                return res;
            }
        } else {
            if ( r2v ) {
                return trySimulate(context, 0, type->baseType);
            } else {
                return trySimulate(context, 0, Type::none);
            }
        }
    }

    SimNode * ExprSafeAt::trySimulate (Context &, uint32_t, Type) const {
        return nullptr;
    }

    SimNode * ExprSafeAt::simulate (Context & context) const {
        if ( subexpr->type->isPointer() ) {
            const auto & seT = subexpr->type->firstType;
            if ( seT->isGoodArrayType() ) {
                auto prv = subexpr->simulate(context);
                auto pidx = index->simulate(context);
                uint32_t stride = seT->firstType->getSizeOf();
                return context.code->makeNode<SimNode_SafeArrayAt>(at, prv, pidx, stride, 0);
            } else if ( seT->isGoodTableType() ) {
                auto prv = subexpr->simulate(context);
                auto pidx = index->simulate(context);
                uint32_t valueTypeSize = seT->secondType->getSizeOf();
                return context.code->makeValueNode<SimNode_SafeTableIndex>(seT->firstType->baseType, at, prv, pidx, valueTypeSize, 0);
            } else if ( seT->dim.size() ) {
                uint32_t range = seT->dim[0];
                uint32_t stride = seT->getStride();
                auto prv = subexpr->simulate(context);
                auto pidx = index->simulate(context);
                return context.code->makeNode<SimNode_SafeAt>(at, prv, pidx, stride, 0, range);
            } else if ( seT->isVectorType() ) {
                auto prv = subexpr->simulate(context);
                auto pidx = index->simulate(context);
                uint32_t range = seT->getVectorDim();
                uint32_t stride = type->getSizeOf();
                return context.code->makeNode<SimNode_SafeAt>(at, prv, pidx, stride, 0, range);
            } else {
                DAS_VERIFY(0 && "TODO: safe-at not implemented");
            }
        } else {
            const auto & seT = subexpr->type;
            if ( seT->isGoodArrayType() ) {
                auto prv = subexpr->simulate(context);
                auto pidx = index->simulate(context);
                uint32_t stride = seT->firstType->getSizeOf();
                return context.code->makeNode<SimNode_SafeArrayAt>(at, prv, pidx, stride, 0);
            } else if ( subexpr->type->isGoodTableType() ) {
                auto prv = subexpr->simulate(context);
                auto pidx = index->simulate(context);
                uint32_t valueTypeSize = seT->secondType->getSizeOf();
                return context.code->makeValueNode<SimNode_SafeTableIndex>(seT->firstType->baseType, at, prv, pidx, valueTypeSize, 0);
            } else if ( seT->dim.size() ) {
                uint32_t range = seT->dim[0];
                uint32_t stride = seT->getStride();
                auto prv = subexpr->simulate(context);
                auto pidx = index->simulate(context);
                return context.code->makeNode<SimNode_SafeAt>(at, prv, pidx, stride, 0, range);
            } else if ( seT->isVectorType() && seT->ref ) {
                auto prv = subexpr->simulate(context);
                auto pidx = index->simulate(context);
                uint32_t range = seT->getVectorDim();
                uint32_t stride = type->getSizeOf();
                return context.code->makeNode<SimNode_SafeAt>(at, prv, pidx, stride, 0, range);
            } else {
                DAS_VERIFY(0 && "TODO: safe-at not implemented");
            }
        }
        return nullptr;
    }

    vector<SimNode *> ExprBlock::collectExpressions ( Context & context,
                                                     const vector<ExpressionPtr> & lis,
                                                     das_map<int32_t,uint32_t> * ofsmap ) const {
        vector<SimNode *> simlist;
        for ( auto & node : lis ) {
            if ( node->rtti_isLet()) {
                auto pLet = static_pointer_cast<ExprLet>(node);
                auto letInit = ExprLet::simulateInit(context, pLet.get());
                simlist.insert(simlist.end(), letInit.begin(), letInit.end());
                continue;
            }
            if ( node->rtti_isLabel() ) {
                if ( ofsmap ) {
                    auto lnode = static_pointer_cast<ExprLabel>(node);
                    (*ofsmap)[lnode->label] = uint32_t(simlist.size());
                }
                continue;
            }
            if ( auto simE = node->simulate(context) ) {
                simlist.push_back(simE);
            }
        }
        return simlist;
    }

    void ExprBlock::simulateFinal ( Context & context, SimNode_Final * block ) const {
        vector<SimNode *> simFList = collectExpressions(context, finalList);
        block->totalFinal = int(simFList.size());
        if ( block->totalFinal ) {
            block->finalList = (SimNode **) context.code->allocate(sizeof(SimNode *)*block->totalFinal);
            for ( uint32_t i = 0; i != block->totalFinal; ++i )
                block->finalList[i] = simFList[i];
        }
    }

    void ExprBlock::simulateBlock ( Context & context, SimNode_Block * block ) const {
        das_map<int32_t,uint32_t> ofsmap;
        vector<SimNode *> simlist = collectExpressions(context, list, &ofsmap);
        block->total = int(simlist.size());
        if ( block->total ) {
            block->list = (SimNode **) context.code->allocate(sizeof(SimNode *)*block->total);
            for ( uint32_t i = 0; i != block->total; ++i )
                block->list[i] = simlist[i];
        }
        simulateLabels(context, block, ofsmap);
    }

    void ExprBlock::simulateLabels ( Context & context, SimNode_Block * block, const das_map<int32_t,uint32_t> & ofsmap ) const {
        if ( maxLabelIndex!=-1 ) {
            block->totalLabels = maxLabelIndex + 1;
            block->labels = (uint32_t *) context.code->allocate(block->totalLabels * sizeof(uint32_t));
            for ( uint32_t i=0; i!=block->totalLabels; ++i ) {
                block->labels[i] = -1U;
            }
            for ( auto & it : ofsmap ) {
                block->labels[it.first] = it.second;
            }
        }
    }

    SimNode * ExprBlock::simulate (Context & context) const {
        das_map<int32_t,uint32_t> ofsmap;
        vector<SimNode *> simlist = collectExpressions(context, list, &ofsmap);
        // we memzero block's stack memory, if there is a finally section
        // bad scenario we fight is ( in scope X ; return ; in scope Y )
        if ( finalList.size() ) {
            uint32_t blockDataSize = stackVarBottom - stackVarTop;
            if ( blockDataSize ) {
                for ( const auto & svr : stackCleanVars ) {
                    SimNode * fakeVar = context.code->makeNode<SimNode_GetLocal>(at, svr.first);
                    SimNode * memZ = context.code->makeNode<SimNode_MemZero>(at, fakeVar, svr.second );
                    simlist.insert( simlist.begin(), memZ );
                }
            }
        }
        // TODO: what if list size is 0?
        if ( simlist.size()!=1 || isClosure || finalList.size() ) {
            SimNode_Block * block;
            if ( isClosure ) {
                bool needResult = type!=nullptr && type->baseType!=Type::tVoid;
                bool C0 = !needResult && simlist.size()==1 && finalList.size()==0;
#if DAS_DEBUGGER
                if ( context.thisProgram->getDebugger() ) {
                    block = context.code->makeNode<SimNodeDebug_ClosureBlock>(at, needResult, C0, annotationData);
                } else
#endif
                {
                    block = context.code->makeNode<SimNode_ClosureBlock>(at, needResult, C0, annotationData);
                }
            } else {
                if ( maxLabelIndex!=-1 ) {
#if DAS_DEBUGGER
                    if ( context.thisProgram->getDebugger() ) {
                        block = context.code->makeNode<SimNodeDebug_BlockWithLabels>(at);
                    } else
#endif
                    {
                        block = context.code->makeNode<SimNode_BlockWithLabels>(at);
                    }
                    simulateLabels(context, block, ofsmap);
                } else {
                    if ( finalList.size()==0 ) {
#if DAS_DEBUGGER
                        if ( context.thisProgram->getDebugger() ) {
                            block = context.code->makeNode<SimNodeDebug_BlockNF>(at);
                        } else
#endif
                        {
                            block = context.code->makeNode<SimNode_BlockNF>(at);
                        }
                    } else {
#if DAS_DEBUGGER
                        if ( context.thisProgram->getDebugger() ) {
                            block = context.code->makeNode<SimNodeDebug_Block>(at);
                        } else
#endif
                        {
                            block = context.code->makeNode<SimNode_Block>(at);
                        }
                    }
                }
            }
            block->annotationDataSid = annotationDataSid;
            block->total = int(simlist.size());
            if ( block->total ) {
                block->list = (SimNode **) context.code->allocate(sizeof(SimNode *)*block->total);
                for ( uint32_t i = 0; i != block->total; ++i )
                    block->list[i] = simlist[i];
            }
            if ( !inTheLoop ) {
                simulateFinal(context, block);
            }
            return block;
        } else {
            return simlist[0];
        }
    }

    SimNode * ExprSwizzle::trySimulate (Context & context, uint32_t extraOffset, Type r2vType ) const {
        if ( !value->type->ref ) {
            return nullptr;
        }
        uint32_t offset = fields[0] * sizeof(float);
        if ( auto chain = value->trySimulate(context, extraOffset + offset, r2vType) ) {
            return chain;
        }
        auto simV = value->simulate(context);
        if ( r2vType!=Type::none ) {
            return context.code->makeValueNode<SimNode_FieldDerefR2V>(r2vType,at,simV,offset + extraOffset);
        } else {
            return context.code->makeNode<SimNode_FieldDeref>(at,simV,offset + extraOffset);
        }
    }

    SimNode * ExprSwizzle::simulate (Context & context) const {
        if ( !type->ref ) {
            bool seq = TypeDecl::isSequencialMask(fields);
            if (seq && value->type->ref) {
                return trySimulate(context, 0, type->baseType);
            } else {
                auto fsz = fields.size();
                uint8_t fs[4];
                fs[0] = fields[0];
                fs[1] = fsz >= 2 ? fields[1] : fields[0];
                fs[2] = fsz >= 3 ? fields[2] : fields[0];
                fs[3] = fsz >= 4 ? fields[3] : fields[0];
                auto simV = value->simulate(context);
                return context.code->makeNode<SimNode_Swizzle>(at, simV, fs);
            }
        } else {
            return trySimulate(context, 0, r2v ? type->baseType : Type::none);
        }
    }

    SimNode * ExprField::simulate (Context & context) const {
        if ( value->type->isBitfield() ) {
            auto simV = value->simulate(context);
            uint32_t mask = 1u << fieldIndex;
            return context.code->makeNode<SimNode_GetBitField>(at, simV, mask);
        } else if ( !field  && fieldIndex==-1 ) {
            if ( r2v ) {
                auto resN = annotation->simulateGetFieldR2V(name, context, at, value);
                if ( !resN ) {
                    context.thisProgram->error("integration error, simulateGetFieldR2V returned null", "", "",
                                               at, CompilationError::missing_node );
                }
                return resN;
            } else {
                auto resN = annotation->simulateGetField(name, context, at, value);
                if ( !resN ) {
                    context.thisProgram->error("integration error, simulateGetField returned null", "", "",
                                               at, CompilationError::missing_node );
                }
                return resN;
            }
        } else {
            return trySimulate(context, 0, r2v ? type->baseType : Type::none);
        }
    }

    SimNode * ExprField::trySimulate (Context & context, uint32_t extraOffset, Type r2vType ) const {
        if ( !field && fieldIndex==-1 ) {
            return nullptr;
        }
        if ( value->type->isBitfield() ) {
            return nullptr;
        }
        int fieldOffset = -1;
        if ( fieldIndex != - 1 ) {
            if ( value->type->isPointer() ) {
                if ( value->type->firstType->isVariant() ) {
                    fieldOffset = value->type->firstType->getVariantFieldOffset(fieldIndex);
                } else {
                    fieldOffset = value->type->firstType->getTupleFieldOffset(fieldIndex);
                }
            } else {
                if ( value->type->isVariant() ) {
                    fieldOffset = value->type->getVariantFieldOffset(fieldIndex);
                } else {
                    fieldOffset = value->type->getTupleFieldOffset(fieldIndex);
                }
            }
        } else {
            DAS_ASSERTF(field, "field can't be null");
            if (!field) return nullptr;
            fieldOffset = field->offset;
        }
        DAS_ASSERTF(fieldOffset>=0,"field offset is somehow not there");
        if (value->type->isPointer()) {
            if ( unsafeDeref ) {
                auto simV = value->simulate(context);
                if (r2vType != Type::none) {
                    return context.code->makeValueNode<SimNode_FieldDerefR2V>(r2vType, at, simV, fieldOffset + extraOffset);
                }
                else {
                    return context.code->makeNode<SimNode_FieldDeref>(at, simV, fieldOffset + extraOffset);
                }
            } else {
                auto simV = value->simulate(context);
                if (r2vType != Type::none) {
                    return context.code->makeValueNode<SimNode_PtrFieldDerefR2V>(r2vType, at, simV, fieldOffset + extraOffset);
                }
                else {
                    return context.code->makeNode<SimNode_PtrFieldDeref>(at, simV, fieldOffset + extraOffset);
                }
            }
        } else {
            if ( auto chain = value->trySimulate(context, extraOffset + fieldOffset, r2vType) ) {
                return chain;
            }
            auto simV = value->simulate(context);
            if ( r2vType!=Type::none ) {
                return context.code->makeValueNode<SimNode_FieldDerefR2V>(r2vType, at, simV, extraOffset + fieldOffset);
            } else {
                return context.code->makeNode<SimNode_FieldDeref>(at, simV, extraOffset + fieldOffset);
            }
        }
    }

    SimNode * ExprIsVariant::simulate(Context & context) const {
        DAS_ASSERT(fieldIndex != -1);
        return context.code->makeNode<SimNode_IsVariant>(at, value->simulate(context), fieldIndex);
    }

    SimNode * ExprAsVariant::simulate (Context & context) const {
        int fieldOffset = value->type->getVariantFieldOffset(fieldIndex);
        auto simV = value->simulate(context);
        if ( r2v ) {
            return context.code->makeValueNode<SimNode_VariantFieldDerefR2V>(type->baseType, at, simV, fieldOffset, fieldIndex);
        } else {
            return context.code->makeNode<SimNode_VariantFieldDeref>(at, simV, fieldOffset, fieldIndex);
        }
    }

    SimNode * ExprSafeAsVariant::simulate (Context & context) const {
        int fieldOffset = value->type->isPointer() ?
            value->type->firstType->getVariantFieldOffset(fieldIndex) :
            value->type->getVariantFieldOffset(fieldIndex);
        auto simV = value->simulate(context);
        if ( skipQQ ) {
            return context.code->makeNode<SimNode_SafeVariantFieldDerefPtr>(at,simV,fieldOffset, fieldIndex);
        } else {
            return context.code->makeNode<SimNode_SafeVariantFieldDeref>(at,simV,fieldOffset, fieldIndex);
        }
    }

    SimNode * ExprSafeField::trySimulate(Context &, uint32_t, Type) const {
        return nullptr;
    }

    SimNode * ExprSafeField::simulate (Context & context) const {
        int fieldOffset = -1;
        if ( !annotation ) {
            if ( fieldIndex != - 1 ) {
                if ( value->type->firstType->isVariant() ) {
                    fieldOffset = value->type->firstType->getVariantFieldOffset(fieldIndex);
                } else {
                    fieldOffset = value->type->firstType->getTupleFieldOffset(fieldIndex);
                }
            } else {
                fieldOffset = field->offset;
            }
            DAS_ASSERTF(fieldOffset>=0,"field offset is somehow not there");
        }
        if ( skipQQ ) {
            if ( annotation ) {
                auto resN = annotation->simulateSafeGetFieldPtr(name, context, at, value);
                if ( !resN ) {
                    context.thisProgram->error("integration error, simulateSafeGetFieldPtr returned null", "", "",
                                               at, CompilationError::missing_node );
                }
                return resN;
            } else {
                return context.code->makeNode<SimNode_SafeFieldDerefPtr>(at,value->simulate(context),fieldOffset);
            }
        } else {
            if ( annotation ) {
                auto resN = annotation->simulateSafeGetField(name, context, at, value);
                if ( !resN ) {
                    context.thisProgram->error("integration error, simulateSafeGetField returned null", "", "",
                                               at, CompilationError::missing_node );
                }
                return resN;
            } else {
                return context.code->makeNode<SimNode_SafeFieldDeref>(at,value->simulate(context),fieldOffset);
            }
        }
    }

    SimNode * ExprStringBuilder::simulate (Context & context) const {
        SimNode_StringBuilder * pSB = context.code->makeNode<SimNode_StringBuilder>(at);
        if ( int nArg = (int) elements.size() ) {
            pSB->arguments = (SimNode **) context.code->allocate(nArg * sizeof(SimNode *));
            pSB->types = (TypeInfo **) context.code->allocate(nArg * sizeof(TypeInfo *));
            pSB->nArguments = nArg;
            for ( int a=0; a!=nArg; ++a ) {
                pSB->arguments[a] = elements[a]->simulate(context);
                pSB->types[a] = context.thisHelper->makeTypeInfo(nullptr, elements[a]->type);
            }
        } else {
            pSB->arguments = nullptr;
            pSB->types = nullptr;
            pSB->nArguments = 0;
        }
        return pSB;
    }

    SimNode * ExprVar::trySimulate (Context & context, uint32_t extraOffset, Type r2vType ) const {
        if ( block ) {
        } else if ( local ) {
            if ( variable->type->ref ) {
                if ( r2vType!=Type::none ) {
                    return context.code->makeValueNode<SimNode_GetLocalRefOffR2V>(r2vType, at,
                                                    variable->stackTop, extraOffset);
                } else {
                    return context.code->makeNode<SimNode_GetLocalRefOff>(at,
                                                    variable->stackTop, extraOffset);
                }
            } else if ( variable->aliasCMRES ) {
                if ( r2vType!=Type::none ) {
                    return context.code->makeValueNode<SimNode_GetCMResOfsR2V>(r2vType, at,extraOffset);
                } else {
                    return context.code->makeNode<SimNode_GetCMResOfs>(at, extraOffset);
                }
            } else {
                if ( r2vType!=Type::none ) {
                    return context.code->makeValueNode<SimNode_GetLocalR2V>(r2vType, at,
                                                                            variable->stackTop + extraOffset);
                } else {
                    return context.code->makeNode<SimNode_GetLocal>(at, variable->stackTop + extraOffset);
                }
            }
        } else if ( argument ) {
            if ( variable->type->isPointer() && variable->type->isRef() ) {
                return nullptr;
            } else if ( variable->type->isPointer() ) {
                if ( r2vType!=Type::none ) {
                    return context.code->makeValueNode<SimNode_GetArgumentRefOffR2V>(r2vType, at, argumentIndex, extraOffset);
                } else {
                    return context.code->makeNode<SimNode_GetArgumentRefOff>(at, argumentIndex, extraOffset);
                }
            } else if (variable->type->isRef()) {
                if ( r2vType!=Type::none ) {
                    return context.code->makeValueNode<SimNode_GetArgumentRefOffR2V>(r2vType, at, argumentIndex, extraOffset);
                } else {
                    return context.code->makeNode<SimNode_GetArgumentRefOff>(at, argumentIndex, extraOffset);
                }
            }
        } else { // global

        }
        return nullptr;
    }

    SimNode * ExprVar::simulate (Context & context) const {
        if ( block ) {
            auto blk = pBlock;
            if (variable->type->isRef()) {
                if (r2v && !type->isRefType()) {
                    if ( thisBlock ) {
                        return context.code->makeValueNode<SimNode_GetThisBlockArgumentR2V>(type->baseType, at, argumentIndex);
                    } else {
                        return context.code->makeValueNode<SimNode_GetBlockArgumentR2V>(type->baseType, at, argumentIndex, blk->stackTop);
                    }
                } else {
                    if ( thisBlock ) {
                        return context.code->makeNode<SimNode_GetThisBlockArgument>(at, argumentIndex);
                    } else {
                        return context.code->makeNode<SimNode_GetBlockArgument>(at, argumentIndex, blk->stackTop);
                    }
                }
            } else {
                if (r2v && !type->isRefType()) {
                    if ( thisBlock ) {
                        return context.code->makeNode<SimNode_GetThisBlockArgument>(at, argumentIndex);
                    } else {
                        return context.code->makeNode<SimNode_GetBlockArgument>(at, argumentIndex, blk->stackTop);
                    }
                }
                else {
                    if ( thisBlock ) {
                        return context.code->makeNode<SimNode_GetThisBlockArgumentRef>(at, argumentIndex);
                    } else {
                        return context.code->makeNode<SimNode_GetBlockArgumentRef>(at, argumentIndex, blk->stackTop);
                    }
                }
            }
        } else if ( local ) {
            if ( r2v ) {
                return trySimulate(context, 0, type->baseType);
            } else {
                return trySimulate(context, 0, Type::none);
            }
        } else if ( argument) {
            if (variable->type->isRef()) {
                if (r2v && !type->isRefType()) {
                    return context.code->makeValueNode<SimNode_GetArgumentR2V>(type->baseType, at, argumentIndex);
                } else {
                    return context.code->makeNode<SimNode_GetArgument>(at, argumentIndex);
                }
            } else {
                if (r2v && !type->isRefType()) {
                    return context.code->makeNode<SimNode_GetArgument>(at, argumentIndex);
                }
                else {
                    return context.code->makeNode<SimNode_GetArgumentRef>(at, argumentIndex);
                }
            }
        } else {
            assert(variable->index >= 0 && "using variable which is not used. how?");
            uint32_t mnh = variable->getMangledNameHash();
            if ( variable->global_shared ) {
                if ( r2v ) {
                    return context.code->makeValueNode<SimNode_GetSharedR2V>(type->baseType, at, variable->stackTop, mnh);
                } else {
                    return context.code->makeNode<SimNode_GetShared>(at, variable->stackTop, mnh);
                }
            } else {
                if ( r2v ) {
                    return context.code->makeValueNode<SimNode_GetGlobalR2V>(type->baseType, at, variable->stackTop, mnh);
                } else {
                    return context.code->makeNode<SimNode_GetGlobal>(at, variable->stackTop, mnh);
                }
            }
        }
    }

    SimNode * ExprOp1::simulate (Context & context) const {
        vector<ExpressionPtr> sarguments = { subexpr };
        if ( func->builtIn && !func->callBased ) {
            auto pSimOp1 = static_cast<SimNode_Op1 *>(func->makeSimNode(context,sarguments));
            pSimOp1->debugInfo = at;
            pSimOp1->x = subexpr->simulate(context);
            return pSimOp1;
        } else {
            auto pCall = static_cast<SimNode_CallBase *>(func->makeSimNode(context,sarguments));
            pCall->debugInfo = at;
            pCall->fnPtr = context.getFunction(func->index);
            pCall->fnIndex = func->index;
            pCall->arguments = (SimNode **) context.code->allocate(1 * sizeof(SimNode *));
            pCall->nArguments = 1;
            pCall->arguments[0] = subexpr->simulate(context);
            pCall->cmresEval = context.code->makeNode<SimNode_GetLocal>(at,stackTop);
            return pCall;
        }
    }

    SimNode * ExprOp2::simulate (Context & context) const {
        vector<ExpressionPtr> sarguments = { left, right };
        if ( func->builtIn && !func->callBased ) {
            auto pSimOp2 = static_cast<SimNode_Op2 *>(func->makeSimNode(context,sarguments));
            pSimOp2->debugInfo = at;
            pSimOp2->l = left->simulate(context);
            pSimOp2->r = right->simulate(context);
            return pSimOp2;
        } else {
            auto pCall = static_cast<SimNode_CallBase *>(func->makeSimNode(context,sarguments));
            pCall->debugInfo = at;
            pCall->fnPtr = context.getFunction(func->index);
            pCall->fnIndex = func->index;
            pCall->arguments = (SimNode **) context.code->allocate(2 * sizeof(SimNode *));
            pCall->nArguments = 2;
            pCall->arguments[0] = left->simulate(context);
            pCall->arguments[1] = right->simulate(context);
            pCall->cmresEval = context.code->makeNode<SimNode_GetLocal>(at,stackTop);
            return pCall;
        }
    }

    SimNode * ExprOp3::simulate (Context & context) const {
        return context.code->makeNode<SimNode_IfThenElse>(at,
                                                    subexpr->simulate(context),
                                                    left->simulate(context),
                                                    right->simulate(context));
    }

    SimNode * ExprMove::simulate (Context & context) const {
        auto retN = makeMove(at,context,left,right);
        if ( !retN ) {
            context.thisProgram->error("internal compilation error, can't generate move", "", "", at);
        }
        return retN;
    }

    SimNode * ExprClone::simulate (Context & context) const {
        SimNode * retN = nullptr;
        if ( left->type->isHandle() ) {
            auto lN = left->simulate(context);
            auto rN = right->simulate(context);
            retN = left->type->annotation->simulateClone(context, at, lN, rN);
        } else if ( left->type->canCopy() ) {
            retN = makeCopy(at, context, left, right );
        } else {
            retN = nullptr;
        }
        if ( !retN ) {
            context.thisProgram->error("internal compilation error, can't generate clone", "", "", at);
        }
        return retN;
    }


    SimNode * ExprCopy::simulate (Context & context) const {
        if ( takeOverRightStack ) {
            auto sl = left->simulate(context);
            auto sr = right->simulate(context);
            return context.code->makeNode<SimNode_SetLocalRefAndEval>(at, sl, sr, stackTop);
        } else {
            auto retN = makeCopy(at, context, left, right);
            if ( !retN ) {
                context.thisProgram->error("internal compilation error, can't generate copy", "", "", at);
            }
            return retN;
        }
    }

    SimNode * ExprTryCatch::simulate (Context & context) const {
        return context.code->makeNode<SimNode_TryCatch>(at,
                                                  try_block->simulate(context),
                                                  catch_block->simulate(context));
    }

    SimNode * ExprReturn::simulate (Context & context) const {
        // return string is its own thing
        if (subexpr && subexpr->type && subexpr->rtti_isConstant()) {
            if (subexpr->type->isSimpleType(Type::tString)) {
                auto cVal = static_pointer_cast<ExprConstString>(subexpr);
                char * str = context.constStringHeap->allocateString(cVal->text);
                return context.code->makeNode<SimNode_ReturnConstString>(at, str);
            }
        }
        // now, lets do the standard everything
        bool skipIt = false;
        if ( subexpr && subexpr->rtti_isMakeLocal() ) {
            if ( static_pointer_cast<ExprMakeLocal>(subexpr)->useCMRES ) {
                skipIt = true;
            }
        }
        SimNode * simSubE = (subexpr && !skipIt) ? subexpr->simulate(context) : nullptr;
        if (!subexpr) {
            return context.code->makeNode<SimNode_ReturnNothing>(at);
        } else if ( subexpr->rtti_isConstant() ) {
            auto cVal = static_pointer_cast<ExprConst>(subexpr);
            return context.code->makeNode<SimNode_ReturnConst>(at, cVal->value);
        }
        if ( returnReference ) {
            if ( returnInBlock ) {
                DAS_ASSERTF(simSubE, "internal error. can't be zero");
                return context.code->makeNode<SimNode_ReturnReferenceFromBlock>(at, simSubE);
            } else {
                DAS_ASSERTF(simSubE, "internal error. can't be zero");
                return context.code->makeNode<SimNode_ReturnReference>(at, simSubE);
            }
        } else if ( returnInBlock ) {
            if ( returnCallCMRES ) {
                DAS_ASSERTF(simSubE, "internal error. can't be zero");
                SimNode_CallBase * simRet = (SimNode_CallBase *) simSubE;
                simRet->cmresEval = context.code->makeNode<SimNode_GetBlockCMResOfs>(at,0,stackTop);
                return context.code->makeNode<SimNode_Return>(at, simSubE);
            } else if ( takeOverRightStack ) {
                DAS_ASSERTF(simSubE, "internal error. can't be zero");
                return context.code->makeNode<SimNode_ReturnRefAndEvalFromBlock>(at,
                            simSubE, refStackTop, stackTop);
            } else if ( block->copyOnReturn  ) {
                DAS_ASSERTF(simSubE, "internal error. can't be zero");
                return context.code->makeNode<SimNode_ReturnAndCopyFromBlock>(at,
                            simSubE, subexpr->type->getSizeOf(), stackTop);
            } else if ( block->moveOnReturn ) {
                DAS_ASSERTF(simSubE, "internal error. can't be zero");
                return context.code->makeNode<SimNode_ReturnAndMoveFromBlock>(at,
                    simSubE, subexpr->type->getSizeOf(), stackTop);
            }
        } else if ( subexpr ) {
            if ( returnCallCMRES ) {
                DAS_ASSERTF(simSubE, "internal error. can't be zero");
                SimNode_CallBase * simRet = (SimNode_CallBase *) simSubE;
                simRet->cmresEval = context.code->makeNode<SimNode_GetCMResOfs>(at,0);
                return context.code->makeNode<SimNode_Return>(at, simSubE);
            } else if ( returnCMRES ) {
                // ReturnLocalCMRes
                if ( subexpr->rtti_isMakeLocal() ) {
                    auto mkl = static_pointer_cast<ExprMakeLocal>(subexpr);
                    if ( mkl->useCMRES ) {
                        SimNode_Block * blockT = context.code->makeNode<SimNode_ReturnLocalCMRes>(at);
                        auto simlist = mkl->simulateLocal(context);
                        blockT->total = int(simlist.size());
                        blockT->list = (SimNode **) context.code->allocate(sizeof(SimNode *)*blockT->total);
                        for ( uint32_t i = 0; i != blockT->total; ++i )
                            blockT->list[i] = simlist[i];
                        return blockT;
                    }
                }
                DAS_ASSERTF(simSubE, "internal error. can't be zero");
                return context.code->makeNode<SimNode_Return>(at, simSubE);
            } else if ( takeOverRightStack ) {
                DAS_ASSERTF(simSubE, "internal error. can't be zero");
                return context.code->makeNode<SimNode_ReturnRefAndEval>(at, simSubE, refStackTop);
            } else if ( returnFunc && returnFunc->copyOnReturn ) {
                DAS_ASSERTF(simSubE, "internal error. can't be zero");
                return context.code->makeNode<SimNode_ReturnAndCopy>(at, simSubE, subexpr->type->getSizeOf());
            } else if ( returnFunc && returnFunc->moveOnReturn ) {
                DAS_ASSERTF(simSubE, "internal error. can't be zero");
                return context.code->makeNode<SimNode_ReturnAndMove>(at, simSubE, subexpr->type->getSizeOf());
            }
        }
        DAS_ASSERTF(simSubE, "internal error. can't be zero");
        return context.code->makeNode<SimNode_Return>(at, simSubE);
    }

    SimNode * ExprBreak::simulate (Context & context) const {
        return context.code->makeNode<SimNode_Break>(at);
    }

    SimNode * ExprContinue::simulate (Context & context) const {
        return context.code->makeNode<SimNode_Continue>(at);
    }

    SimNode * ExprIfThenElse::simulate (Context & context) const {
        ExpressionPtr zeroCond;
        bool condIfZero = false;
        bool match0 = matchEquNequZero(cond, zeroCond, condIfZero);
#if DAS_DEBUGGER
        if ( context.thisProgram->getDebugger() ) {
            if ( match0 && zeroCond->type->isWorkhorseType() ) {
                if ( condIfZero ) {
                    if ( if_false ) {
                        return context.code->makeNumericValueNode<SimNodeDebug_IfZeroThenElse>(zeroCond->type->baseType,
                                        at, zeroCond->simulate(context), if_true->simulate(context),
                                                if_false->simulate(context));

                    } else {
                        return context.code->makeNumericValueNode<SimNodeDebug_IfZeroThen>(zeroCond->type->baseType,
                                        at, zeroCond->simulate(context), if_true->simulate(context));
                    }
                } else {
                    if ( if_false ) {
                        return context.code->makeNumericValueNode<SimNodeDebug_IfNotZeroThenElse>(zeroCond->type->baseType,
                                        at, zeroCond->simulate(context), if_true->simulate(context),
                                                if_false->simulate(context));
                    } else {
                        return context.code->makeNumericValueNode<SimNodeDebug_IfNotZeroThen>(zeroCond->type->baseType,
                                        at, zeroCond->simulate(context), if_true->simulate(context));
                    }
                }
            } else {
                // good old if
                if ( if_false ) {
                    return context.code->makeNode<SimNodeDebug_IfThenElse>(at, cond->simulate(context),
                                        if_true->simulate(context), if_false->simulate(context));
                } else {
                    return context.code->makeNode<SimNodeDebug_IfThen>(at, cond->simulate(context),
                                        if_true->simulate(context));
                }
            }
        } else
#endif
        {
            if ( match0 && zeroCond->type->isWorkhorseType() ) {
                if ( condIfZero ) {
                    if ( if_false ) {
                        return context.code->makeNumericValueNode<SimNode_IfZeroThenElse>(zeroCond->type->baseType,
                                        at, zeroCond->simulate(context), if_true->simulate(context),
                                                if_false->simulate(context));

                    } else {
                        return context.code->makeNumericValueNode<SimNode_IfZeroThen>(zeroCond->type->baseType,
                                        at, zeroCond->simulate(context), if_true->simulate(context));
                    }
                } else {
                    if ( if_false ) {
                        return context.code->makeNumericValueNode<SimNode_IfNotZeroThenElse>(zeroCond->type->baseType,
                                        at, zeroCond->simulate(context), if_true->simulate(context),
                                                if_false->simulate(context));
                    } else {
                        return context.code->makeNumericValueNode<SimNode_IfNotZeroThen>(zeroCond->type->baseType,
                                        at, zeroCond->simulate(context), if_true->simulate(context));
                    }
                }
            } else {
                // good old if
                if ( if_false ) {
                    return context.code->makeNode<SimNode_IfThenElse>(at, cond->simulate(context),
                                        if_true->simulate(context), if_false->simulate(context));
                } else {
                    return context.code->makeNode<SimNode_IfThen>(at, cond->simulate(context),
                                        if_true->simulate(context));
                }
            }
        }
    }

    SimNode * ExprWith::simulate (Context & context) const {
        return body->simulate(context);
    }

    void ExprWhile::simulateFinal ( Context & context, const ExpressionPtr & bod, SimNode_Block * blk ) {
        if ( bod->rtti_isBlock() ) {
            auto pBlock = static_pointer_cast<ExprBlock>(bod);
            pBlock->simulateBlock(context, blk);
            pBlock->simulateFinal(context, blk);
        } else {
            context.thisProgram->error("internal error, expecting block", "", "", bod->at);
        }
    }

    SimNode * ExprWhile::simulate (Context & context) const {
#if DAS_DEBUGGER
        if ( context.thisProgram->getDebugger() ) {
            auto node = context.code->makeNode<SimNodeDebug_While>(at, cond->simulate(context));
            simulateFinal(context, body, node);
            return node;
        } else
#endif
        {
            auto node = context.code->makeNode<SimNode_While>(at, cond->simulate(context));
            simulateFinal(context, body, node);
            return node;
        }
    }

    SimNode * ExprUnsafe::simulate (Context & context) const {
        return body->simulate(context);
    }

    SimNode * ExprFor::simulate (Context & context) const {
        // determine iteration types
        bool nativeIterators = false;
        bool fixedArrays = false;
        bool dynamicArrays = false;
        bool stringChars = false;
        bool rangeBase = false;
        int32_t fixedSize = INT32_MAX;
        for ( auto & src : sources ) {
            if ( !src->type ) continue;
            if ( src->type->isArray() ) {
                fixedSize = das::min(fixedSize, src->type->dim[0]);
                fixedArrays = true;
            } else if ( src->type->isGoodArrayType() ) {
                dynamicArrays = true;
            } else if ( src->type->isGoodIteratorType() ) {
                nativeIterators = true;
            } else if ( src->type->isHandle() ) {
                nativeIterators = true;
            } else if ( src->type->isRange() ) {
                rangeBase = true;
            } else if ( src->type->isString() ) {
                stringChars = true;
            }
        }
        // create loops based on
        int  total = int(sources.size());
        int  sourceTypes = int(dynamicArrays) + int(fixedArrays) + int(rangeBase) + int(stringChars);
        bool hybridRange = rangeBase && (total>1);
        if ( (sourceTypes>1) || hybridRange || nativeIterators || stringChars ) {
            SimNode_ForWithIteratorBase * result;
#if DAS_DEBUGGER
            if ( context.thisProgram->getDebugger() ) {
                result = (SimNode_ForWithIteratorBase *) context.code->makeNodeUnrollNZ_FOR<SimNodeDebug_ForWithIterator>(total, at);
            } else
#endif
            {
                result = (SimNode_ForWithIteratorBase *) context.code->makeNodeUnrollNZ_FOR<SimNode_ForWithIterator>(total, at);
            }
            for ( int t=0; t!=total; ++t ) {
                if ( sources[t]->type->isGoodIteratorType() ) {
                    result->source_iterators[t] = context.code->makeNode<SimNode_Seq2Iter>(
                        sources[t]->at,
                        sources[t]->simulate(context));
                } else if ( sources[t]->type->isGoodArrayType() ) {
                    result->source_iterators[t] = context.code->makeNode<SimNode_GoodArrayIterator>(
                        sources[t]->at,
                        sources[t]->simulate(context),
                        sources[t]->type->firstType->getStride());
                } else if ( sources[t]->type->isRange() ) {
                    result->source_iterators[t] = context.code->makeNode<SimNode_RangeIterator>(
                        sources[t]->at,
                        sources[t]->simulate(context));
                } else if ( sources[t]->type->isString() ) {
                    result->source_iterators[t] = context.code->makeNode<SimNode_StringIterator>(
                        sources[t]->at,
                        sources[t]->simulate(context));
                } else if ( sources[t]->type->isHandle() ) {
                    if ( !result ) {
                        context.thisProgram->error("integration error, simulateGetIterator returned null", "", "",
                                                   at, CompilationError::missing_node );
                        return nullptr;
                    } else {
                        result->source_iterators[t] = sources[t]->type->annotation->simulateGetIterator(
                            context,
                            sources[t]->at,
                            sources[t]
                        );
                    }
                } else if ( sources[t]->type->dim.size() ) {
                    result->source_iterators[t] = context.code->makeNode<SimNode_FixedArrayIterator>(
                        sources[t]->at,
                        sources[t]->simulate(context),
                        sources[t]->type->dim[0],
                        sources[t]->type->getStride());
                } else {
                    DAS_ASSERTF(0, "we should not be here. we are doing iterator for on an unsupported type.");
                    context.thisProgram->error("internal compilation error, generating for-with-iterator", "", "", at);
                    return nullptr;
                }
                result->stackTop[t] = iteratorVariables[t]->stackTop;
            }
            ExprWhile::simulateFinal(context, body, result);
            return result;
        } else {
            auto flagsE = body->getEvalFlags();
            bool NF = flagsE == 0;
            SimNode_ForBase * result;
            assert(body->rtti_isBlock() && "there would be internal error otherwise");
            auto subB = static_pointer_cast<ExprBlock>(body);
            bool loop1 = (subB->list.size() == 1);
#if DAS_DEBUGGER
            if ( context.thisProgram->getDebugger() ) {
                if ( dynamicArrays ) {
                    if (loop1) {
                        result = (SimNode_ForBase *) context.code->makeNodeUnrollNZ_FOR<SimNodeDebug_ForGoodArray1>(total, at);
                    } else {
                        result = (SimNode_ForBase *) context.code->makeNodeUnrollNZ_FOR<SimNodeDebug_ForGoodArray>(total, at);
                    }
                } else if ( fixedArrays ) {
                    if (loop1) {
                        result = (SimNode_ForBase *)context.code->makeNodeUnrollNZ_FOR<SimNodeDebug_ForFixedArray1>(total, at);
                    } else {
                        result = (SimNode_ForBase *)context.code->makeNodeUnrollNZ_FOR<SimNodeDebug_ForFixedArray>(total, at);
                    }
                } else if ( rangeBase ) {
                    assert(total==1 && "simple range on 1 loop only");
                    if ( NF ) {
                        if (loop1) {
                            result = context.code->makeNode<SimNodeDebug_ForRangeNF1>(at);
                        } else {
                            result = context.code->makeNode<SimNodeDebug_ForRangeNF>(at);
                        }
                    } else {
                        if (loop1) {
                            result = context.code->makeNode<SimNodeDebug_ForRange1>(at);
                        } else {
                            result = context.code->makeNode<SimNodeDebug_ForRange>(at);
                        }
                    }
                } else {
                    DAS_ASSERTF(0, "we should not be here yet. logic above assumes optimized for path of some kind.");
                    context.thisProgram->error("internal compilation error, generating for", "", "", at);
                    return nullptr;
                }
            } else
#endif
            {
                if ( dynamicArrays ) {
                    if (loop1) {
                        result = (SimNode_ForBase *) context.code->makeNodeUnrollNZ_FOR<SimNode_ForGoodArray1>(total, at);
                    } else {
                        result = (SimNode_ForBase *) context.code->makeNodeUnrollNZ_FOR<SimNode_ForGoodArray>(total, at);
                    }
                } else if ( fixedArrays ) {
                    if (loop1) {
                        result = (SimNode_ForBase *)context.code->makeNodeUnrollNZ_FOR<SimNode_ForFixedArray1>(total, at);
                    } else {
                        result = (SimNode_ForBase *)context.code->makeNodeUnrollNZ_FOR<SimNode_ForFixedArray>(total, at);
                    }
                } else if ( rangeBase ) {
                    assert(total==1 && "simple range on 1 loop only");
                    if ( NF ) {
                        if (loop1) {
                            result = context.code->makeNode<SimNode_ForRangeNF1>(at);
                        } else {
                            result = context.code->makeNode<SimNode_ForRangeNF>(at);
                        }
                    } else {
                        if (loop1) {
                            result = context.code->makeNode<SimNode_ForRange1>(at);
                        } else {
                            result = context.code->makeNode<SimNode_ForRange>(at);
                        }
                    }
                } else {
                    DAS_ASSERTF(0, "we should not be here yet. logic above assumes optimized for path of some kind.");
                    context.thisProgram->error("internal compilation error, generating for", "", "", at);
                    return nullptr;
                }
            }
            for ( int t=0; t!=total; ++t ) {
                result->sources[t] = sources[t]->simulate(context);
                if ( sources[t]->type->isGoodArrayType() ) {
                    result->strides[t] = sources[t]->type->firstType->getStride();
                } else {
                    result->strides[t] = sources[t]->type->getStride();
                }
                result->stackTop[t] = iteratorVariables[t]->stackTop;
            }
            result->size = fixedSize;
            ExprWhile::simulateFinal(context, body, result);
            return result;
        }
    }

    vector<SimNode *> ExprLet::simulateInit(Context & context, const ExprLet * pLet) {
        vector<SimNode *> simlist;
        simlist.reserve(pLet->variables.size());
        for (auto & var : pLet->variables) {
            SimNode * init;
            if (var->init) {
                init = ExprLet::simulateInit(context, var, true);
            } else if (var->aliasCMRES ) {
                int bytes = var->type->getSizeOf();
                if ( bytes <= 32 ) {
                    init = context.code->makeNodeUnrollNZ<SimNode_InitLocalCMResN>(bytes, pLet->at,0);
                } else {
                    init = context.code->makeNode<SimNode_InitLocalCMRes>(pLet->at,0,bytes);
                }
            } else {
                init = context.code->makeNode<SimNode_InitLocal>(pLet->at, var->stackTop, var->type->getSizeOf());
            }
            if (init)
                simlist.push_back(init);
        }
        return simlist;
    }

    SimNode * ExprLet::simulateInit(Context & context, const VariablePtr & var, bool local) {
        SimNode * get;
        if ( local ) {
            if ( var->init && var->init->rtti_isMakeLocal() ) {
                return var->init->simulate(context);
            } else {
                get = context.code->makeNode<SimNode_GetLocal>(var->init->at, var->stackTop);
            }
        } else {
            if ( var->init && var->init->rtti_isMakeLocal() ) {
                return var->init->simulate(context);
            } else {
                if ( var->global_shared ) {
                    get = context.code->makeNode<SimNode_GetShared>(var->init->at, var->index, var->getMangledNameHash());
                } else {
                    get = context.code->makeNode<SimNode_GetGlobal>(var->init->at, var->index, var->getMangledNameHash());
                }
            }
        }
        if ( var->type->ref ) {
            return context.code->makeNode<SimNode_CopyReference>(var->init->at, get,
                                                                 var->init->simulate(context));
        } else if ( var->init_via_move && var->type->canMove() ) {
            auto varExpr = make_smart<ExprVar>(var->at, var->name);
            varExpr->variable = var;
            varExpr->local = local;
            varExpr->type = make_smart<TypeDecl>(*var->type);
            auto retN = makeMove(var->init->at, context, varExpr, var->init);
            if ( !retN ) {
                context.thisProgram->error("internal compilation error, can't generate move", "", "", var->at);
            }
            return retN;
        } else if ( !var->init_via_move && var->type->canCopy() ) {
            auto varExpr = make_smart<ExprVar>(var->at, var->name);
            varExpr->variable = var;
            varExpr->local = local;
            varExpr->type = make_smart<TypeDecl>(*var->type);
            auto retN = makeCopy(var->init->at, context, varExpr, var->init);
            if ( !retN ) {
                context.thisProgram->error("internal compilation error, can't generate copy", "", "", var->at);
            }
            return retN;
        } else {
            context.thisProgram->error("internal compilation error, initializing variable which can't be copied or moved", "", "", var->at);
            return nullptr;
        }
    }

    SimNode * ExprLet::simulate (Context & context) const {
        auto let = context.code->makeNode<SimNode_Let>(at);
        let->total = (uint32_t) variables.size();
        let->list = (SimNode **) context.code->allocate(let->total * sizeof(SimNode*));
        auto simList = ExprLet::simulateInit(context, this);
        copy(simList.data(), simList.data() + simList.size(), let->list);
        return let;
    }

    SimNode_CallBase * ExprCall::simulateCall (const FunctionPtr & func,
                                               const ExprLooksLikeCall * expr,
                                               Context & context,
                                               SimNode_CallBase * pCall) {
        bool needTypeInfo = false;
        for ( auto & arg : func->arguments ) {
            if ( arg->type->baseType==Type::anyArgument )
                needTypeInfo = true;
        }
        pCall->debugInfo = expr->at;
        DAS_ASSERTF((func->builtIn || func->index>=0), "calling function which is not used. how?");
        pCall->fnPtr = context.getFunction(func->index);
        pCall->fnIndex = func->index;
        if ( int nArg = (int) expr->arguments.size() ) {
            pCall->arguments = (SimNode **) context.code->allocate(nArg * sizeof(SimNode *));
            if ( needTypeInfo ) {
                pCall->types = (TypeInfo **) context.code->allocate(nArg * sizeof(TypeInfo *));
            } else {
                pCall->types = nullptr;
            }
            pCall->nArguments = nArg;
            for ( int a=0; a!=nArg; ++a ) {
                pCall->arguments[a] = expr->arguments[a]->simulate(context);
                if ( pCall->types ) {
                    if ( func->arguments[a]->type->baseType==Type::anyArgument ) {
                        pCall->types[a] = context.thisHelper->makeTypeInfo(nullptr, expr->arguments[a]->type);
                    } else {
                        pCall->types[a] = nullptr;
                    }
                }
            }
        } else {
            pCall->arguments = nullptr;
            pCall->nArguments = 0;
        }
        return pCall;

    }

    SimNode * ExprCall::simulate (Context & context) const {
        auto pCall = static_cast<SimNode_CallBase *>(func->makeSimNode(context,arguments));
        simulateCall(func, this, context, pCall);
        if ( !doesNotNeedSp && stackTop ) {
            pCall->cmresEval = context.code->makeNode<SimNode_GetLocal>(at,stackTop);
        }
        return pCall;
    }

    SimNode * ExprNamedCall::simulate (Context &) const {
        DAS_ASSERTF(false, "we should not be here. named call should be promoted to regular call");
        return nullptr;
    }

    void Program::buildGMNLookup ( Context & context, TextWriter & logs ) {
        das_hash_map<uint32_t, uint32_t> htab;
        for ( int i=0; i!=context.totalVariables; ++i ) {
            auto mnh = context.globalVariables[i].mangledNameHash;
            if ( htab[mnh] ) {
                error("internal compiler error. mangled name hash collision "
                      + string(context.globalVariables[i].name), "", "", LineInfo());
                return;
            }
            htab[mnh] = context.globalVariables[i].offset;
        }
        auto tab = buildLookup(htab, context.tabGMnMask, context.tabGMnRot);
        context.tabGMnSize = uint32_t(tab.size());
        context.tabGMnLookup = (uint32_t *) context.code->allocate(context.tabGMnSize * sizeof(uint32_t));
        memcpy ( context.tabGMnLookup, tab.data(), context.tabGMnSize * sizeof(uint32_t));
        if ( options.getBoolOption("log_gmn_hash",false) ) {
            logs
                << "totalGlobals: " << context.totalVariables << "\n"
                << "tabGMnLookup:" << context.tabGMnSize << "\n"
                << "tabGMnMask:" << context.tabGMnMask << "\n"
                << "tabGMnRot:" << context.tabGMnRot << "\n";
        }
        for ( int i=0; i!=context.totalVariables; ++i ) {
            auto & gvar = context.globalVariables[i];
            uint32_t voffset = context.globalOffsetByMangledName(gvar.mangledNameHash);
            if ( voffset != gvar.offset ) {
                error("internal compiler error. global variable mangled name hash collision "
                        + string(context.functions[i].mangledName), "", "", LineInfo());
                return;
            }
        }
    }

    void Program::buildMNLookup ( Context & context, TextWriter & logs ) {
        das_hash_map<uint32_t, uint32_t> htab;
        for ( int i=0; i!=context.totalFunctions; ++i ) {
            auto mnh = context.functions[i].mangledNameHash;
            if ( htab[mnh] ) {
                error("internal compiler error. mangled name hash collision "
                      + string(context.functions[i].mangledName), "", "", LineInfo());
                return;
            }
            htab[mnh] = i + 1;
        }
        auto tab = buildLookup(htab, context.tabMnMask, context.tabMnRot);
        context.tabMnSize = uint32_t(tab.size());
        if ( options.getBoolOption("log_mn_hash",false) ) {
            logs
                << "totalFunctions: " << context.totalFunctions << "\n"
                << "tabMnLookup:" << context.tabMnSize << "\n"
                << "tabMnMask:" << context.tabMnMask << "\n"
                << "tabMnRot:" << context.tabMnRot << "\n";
        }
        context.tabMnLookup = (uint32_t *) context.code->allocate(context.tabMnSize * sizeof(uint32_t));
        memcpy ( context.tabMnLookup, tab.data(), context.tabMnSize * sizeof(uint32_t));
    }

    void Program::buildADLookup ( Context & context, TextWriter & logs ) {
        das_hash_map<uint32_t,uint64_t>  tabAd;
        for (auto & pm : library.modules ) {
            for(auto s2d : pm->annotationData ) {
                tabAd[s2d.first] = s2d.second;
            }
        }
        if ( tabAd.size() ) {
            auto tab = buildLookup(tabAd, context.tabAdMask, context.tabAdRot);
            context.tabAdSize = uint32_t(tab.size());
            context.tabAdLookup = (uint64_t *) context.code->allocate(context.tabAdSize * sizeof(uint64_t));
            memcpy ( context.tabAdLookup, tab.data(), context.tabAdSize * sizeof(uint64_t));
            if ( options.getBoolOption("log_ad_hash",false) ) {
                logs
                << "tabAdLookup:" << context.tabAdSize << "\n"
                << "tabAdMask:" << context.tabAdMask << "\n"
                << "tabAdRot:" << context.tabAdRot << "\n";
            }
        }
    }

    void Program::makeMacroModule ( TextWriter & logs ) {
        isCompilingMacros = true;
        thisModule->macroContext = make_unique<Context>(getContextStackSize());
        simulate(*thisModule->macroContext, logs);
        isCompilingMacros = false;
    }

    bool Program::simulate ( Context & context, TextWriter & logs, StackAllocator * sharedStack ) {
        isSimulating = true;
        context.thisProgram = this;
        context.persistent = options.getBoolOption("persistent_heap", policies.persistent_heap);
        if ( context.persistent ) {
            context.heap = make_smart<PersistentHeapAllocator>();
            context.stringHeap = make_smart<PersistentStringAllocator>();
        } else {
            context.heap = make_smart<LinearHeapAllocator>();
            context.stringHeap = make_smart<LinearStringAllocator>();
        }
        context.heap->setInitialSize ( options.getIntOption("heap_size_hint", policies.heap_size_hint) );
        context.stringHeap->setInitialSize ( options.getIntOption("string_heap_size_hint", policies.string_heap_size_hint) );
        context.constStringHeap = make_smart<ConstStringAllocator>();
        if ( globalStringHeapSize ) {
            context.constStringHeap->setInitialSize(globalStringHeapSize);
        }
        DebugInfoHelper helper(context.debugInfo);
        helper.rtti = options.getBoolOption("rtti",policies.rtti);
        context.thisHelper = &helper;
        context.globalVariables = (GlobalVariable *) context.code->allocate( totalVariables*sizeof(GlobalVariable) );
        context.globalsSize = 0;
        context.sharedSize = 0;
        if ( totalVariables ) {
            for (auto & pm : library.modules ) {
                for (auto & pvar : pm->globalsInOrder) {
                    if (!pvar->used)
                        continue;
                    DAS_ASSERTF(pvar->index >= 0, "we are simulating variable, which is not used");
                    auto & gvar = context.globalVariables[pvar->index];
                    gvar.name = context.code->allocateName(pvar->name);
                    gvar.size = pvar->type->getSizeOf();
                    gvar.debugInfo = helper.makeVariableDebugInfo(*pvar);
                    gvar.flags = 0;
                    if ( pvar->global_shared ) {
                        gvar.offset = pvar->stackTop = context.sharedSize;
                        gvar.shared = true;
                        context.sharedSize = (context.sharedSize + gvar.size + 0xf) & ~0xf;
                    } else {
                        gvar.offset = pvar->stackTop = context.globalsSize;
                        context.globalsSize = (context.globalsSize + gvar.size + 0xf) & ~0xf;
                    }
                    gvar.mangledNameHash = pvar->getMangledNameHash();
                }
            }
        }
        context.globals = (char *) das_aligned_alloc16(context.globalsSize);
        context.shared = (char *) das_aligned_alloc16(context.sharedSize);
        context.sharedOwner = true;
        context.totalVariables = totalVariables;
        context.functions = (SimFunction *) context.code->allocate( totalFunctions*sizeof(SimFunction) );
        context.totalFunctions = totalFunctions;
        if ( totalFunctions ) {
            for (auto & pm : library.modules) {
                for (auto & it : pm->functions) {
                    auto pfun = it.second;
                    if (pfun->index < 0 || !pfun->used)
                        continue;
                    auto & gfun = context.functions[pfun->index];
                    auto mangledName = pfun->getMangledName();
                    gfun.name = context.code->allocateName(pfun->name);
                    gfun.mangledName = context.code->allocateName(mangledName);
                    gfun.code = pfun->simulate(context);
                    gfun.debugInfo = helper.makeFunctionDebugInfo(*pfun);
                    if ( getDebugger() ) {
                        helper.appendLocalVariables(gfun.debugInfo, pfun->body);
                    }
                    gfun.stackSize = pfun->totalStackSize;
                    gfun.mangledNameHash = hash_blockz32((uint8_t *)mangledName.c_str());
                    gfun.aotFunction = nullptr;
                    gfun.flags = 0;
                    gfun.fastcall = pfun->fastCall;
                }
            }
        }
        if ( totalVariables ) {
            for (auto & pm : library.modules ) {
                for (auto & pvar : pm->globalsInOrder) {
                    if (!pvar->used)
                        continue;
                    auto & gvar = context.globalVariables[pvar->index];
                    if ( pvar->init ) {
                        if ( pvar->init->rtti_isMakeLocal() ) {
                            if ( pvar->global_shared ) {
                                auto sl = context.code->makeNode<SimNode_GetShared>(pvar->init->at, pvar->stackTop, pvar->getMangledNameHash());
                                auto sr = ExprLet::simulateInit(context, pvar, false);
                                auto gvari = context.code->makeNode<SimNode_SetLocalRefAndEval>(pvar->init->at, sl, sr, uint32_t(sizeof(Prologue)));
                                auto cndb = context.code->makeNode<SimNode_GetArgument>(pvar->init->at, 1); // arg 1 of init script is "init_globals"
                                gvar.init = context.code->makeNode<SimNode_IfThen>(pvar->init->at, cndb, gvari);
                            } else {
                                auto sl = context.code->makeNode<SimNode_GetGlobal>(pvar->init->at, pvar->stackTop, pvar->getMangledNameHash());
                                auto sr = ExprLet::simulateInit(context, pvar, false);
                                gvar.init = context.code->makeNode<SimNode_SetLocalRefAndEval>(pvar->init->at, sl, sr, uint32_t(sizeof(Prologue)));
                            }
                        } else {
                            gvar.init = ExprLet::simulateInit(context, pvar, false);
                        }
                    } else {
                        gvar.init = nullptr;
                    }
                }
            }
        }
        //
        context.globalInitStackSize = globalInitStackSize;
        buildMNLookup(context, logs);
        buildGMNLookup(context, logs);
        buildADLookup(context, logs);
        context.simEnd();
        // if RTTI is enabled
        if (errors.size()) {
            isSimulating = false;
            return false;
        }
        fusion(context, logs);
        context.relocateCode();
        context.restart();
        // now call annotation simulate
        das_hash_map<int,Function *> indexToFunction;
        for (auto & pm : library.modules) {
            for (auto & it : pm->functions) {
                auto pfun = it.second;
                if (pfun->index < 0 || !pfun->used)
                    continue;
                auto & gfun = context.functions[pfun->index];
                for ( const auto & an : pfun->annotations ) {
                    auto fna = static_pointer_cast<FunctionAnnotation>(an->annotation);
                    if (!fna->simulate(&context, &gfun)) {
                        error("function " + pfun->describe() + " annotation " + fna->name + " simulation failed", "", "",
                            LineInfo(), CompilationError::cant_initialize);
                    }
                }
                indexToFunction[pfun->index] = pfun.get();
            }
        }
        // verify code and string heaps
        DAS_ASSERTF(context.code->depth()<=1, "code must come in one page");
        DAS_ASSERTF(context.constStringHeap->depth()<=1, "strings must come in one page");
        context.stringHeap->setIntern(options.getBoolOption("intern_strings", policies.intern_strings));
        // log all functions
        if ( options.getBoolOption("log_nodes",false) ) {
            bool displayHash = options.getBoolOption("log_nodes_aot_hash",false);
            for ( int i=0; i!=context.totalVariables; ++i ) {
                auto & pv = context.globalVariables[i];
                if ( pv.init ) {
                    logs << "// init " << pv.name << "\n";
                    printSimNode(logs, &context, pv.init, displayHash);
                    logs << "\n\n";
                }
            }
            for ( int i=0; i!=context.totalFunctions; ++i ) {
                if (SimFunction * fn = context.getFunction(i)) {
                    logs << "// " << fn->name << " " << fn->mangledName << "\n";
                    printSimFunction(logs, &context, indexToFunction[i], fn->code, displayHash);
                    logs << "\n\n";
                }
            }
        }
        // run init script and restart
        if (!context.runWithCatch([&]() {
            if (context.stack.size()) {
                context.runInitScript();
            } else if ( sharedStack ) {
                SharedStackGuard guard(context, *sharedStack);
                context.runInitScript();
            } else {
                auto ssz = getContextStackSize();
                StackAllocator stack(ssz ? ssz : 16384);    // at least some stack
                SharedStackGuard guard(context, stack);
                context.runInitScript();
            }
        })) {
            string exc = context.getException();
            error("exception during init script", exc, "", LineInfo(), CompilationError::cant_initialize);
        }
        context.restart();
        if (options.getBoolOption("log_mem",false)) {
            logs << "globals       " << context.getGlobalSize() << "\n";
            logs << "shared        " << context.getSharedSize() << "\n";
            logs << "stack         " << context.stack.size() << "\n";
            logs << "code          " << context.code->bytesAllocated() << " in "<< context.code->depth()
                << " pages (" << context.code->totalAlignedMemoryAllocated() << ")\n";
            logs << "const strings " << context.constStringHeap->bytesAllocated() << " in "<< context.constStringHeap->depth()
                << " pages (" << context.constStringHeap->totalAlignedMemoryAllocated() << ")\n";
            logs << "debug         " << context.debugInfo->bytesAllocated() << " (" <<
                context.debugInfo->totalAlignedMemoryAllocated() << ")\n";
            logs << "heap          " << context.heap->bytesAllocated() << " in "<< context.heap->depth()
                << " pages (" << context.heap->totalAlignedMemoryAllocated() << ")\n";
            logs << "string        " << context.stringHeap->bytesAllocated() << " in "<< context.stringHeap->depth()
                << " pages(" << context.stringHeap->totalAlignedMemoryAllocated() << ")\n";
            logs << "shared        " << context.getSharedMemorySize() << "\n";
            logs << "unique        " << context.getUniqueMemorySize() << "\n";
        }
        // log CPP
        if (options.getBoolOption("log_cpp")) {
            aotCpp(context,logs);
            registerAotCpp(logs,context);
        }
        if ( !options.getBoolOption("rtti",policies.rtti) ) {
            context.thisProgram = nullptr;
        }
        context.debugger = getDebugger();
        isSimulating = false;
        return errors.size() == 0;
    }

    uint64_t Program::getInitSemanticHashWithDep( uint64_t initHash ) const {
        vector<const Variable *> globs;
        globs.reserve(totalVariables);
        for (auto & pm : library.modules) {
            for (auto & var : pm->globalsInOrder) {
                if (var->used) {
                    globs.push_back(var.get());
                }
            }
        }
        uint64_t res = getVariableListAotHash(globs, initHash);
        // add init functions to dependencies
        const uint64_t fnv_prime = 1099511628211ul;
        for (auto& pm : library.modules) {
            for (auto& it : pm->functions) {
                auto pfun = it.second;
                if (pfun->index < 0 || !pfun->used || !pfun->init)
                    continue;
                res = (res ^ pfun->aotHash) * fnv_prime;
            }
        }
        return res;
    }

    void Program::linkCppAot ( Context & context, AotLibrary & aotLib, TextWriter & logs ) {
        bool logIt = options.getBoolOption("log_aot",false);

        // make list of functions
        vector<Function *> fnn; fnn.reserve(totalFunctions);
        das_hash_map<int,Function *> indexToFunction;
        for (auto & pm : library.modules) {
            for (auto & it : pm->functions) {
                auto pfun = it.second;
                if (pfun->index < 0 || !pfun->used)
                    continue;
                fnn.push_back(pfun.get());
                indexToFunction[pfun->index] = pfun.get();
            }
        }

        for ( int fni=0; fni!=context.totalFunctions; ++fni ) {
            if ( !fnn[fni]->noAot ) {
                SimFunction & fn = context.functions[fni];
                fnn[fni]->hash = getFunctionHash(fnn[fni], fn.code, &context);
            }
        }

        for ( int fni=0; fni!=context.totalFunctions; ++fni ) {
            if ( !fnn[fni]->noAot ) {
                SimFunction & fn = context.functions[fni];
                uint64_t semHash = fnn[fni]->aotHash = getFunctionAotHash(fnn[fni]);
                auto it = aotLib.find(semHash);
                if ( it != aotLib.end() ) {
                    fn.code = (it->second)(context);
                    fn.aot = true;
                    if ( logIt ) logs << fn.mangledName << " AOT=0x" << HEX << semHash << DEC << "\n";
                    auto fcb = (SimNode_CallBase *) fn.code;
                    fn.aotFunction = fcb->aotFunction;
                } else {
                    if ( logIt ) logs << "NOT FOUND " << fn.mangledName << " AOT=0x" << HEX << semHash << DEC << "\n";
                    if ( policies.fail_on_no_aot ) {
                        TextWriter tp;
                        tp << "semantic hash is " << HEX << semHash << DEC << "\n";
                        printSimFunction(tp, &context, indexToFunction[fni], fn.code, true);
                        error("AOT linking failed for " + string(fn.mangledName), tp.str(), "",
                            LineInfo(), CompilationError::missing_aot);
                    }
                }
            }
        }
        if ( context.totalVariables ) {
            uint64_t semHash = context.getInitSemanticHash();
            semHash = getInitSemanticHashWithDep(semHash);
            auto it = aotLib.find(semHash);
            if ( it != aotLib.end() ) {
                context.aotInitScript = (it->second)(context);
                if ( logIt ) logs << "INIT SCRIPT AOT=0x" << HEX << semHash << DEC << "\n";
            } else {
                if ( logIt ) logs << "INIT SCRIPT NOT FOUND, AOT=0x" << HEX << semHash << DEC << "\n";
            }
        }
    }
}

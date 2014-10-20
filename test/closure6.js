    let ir = llvm.IRBuilder;
    let LLVMIRVisitor = function () {
            let LLVMIRVisitor = void 0;
            LLVMIRVisitor = function LLVMIRVisitor() {
                let args = gatherRest('args', 0);
            };
            LLVMIRVisitor.prototype.visitSwitch = function visitSwitch(n) {
                let n = argPresent(1) ? getArg(0) : void 0;
                let _this_0 = this;
                let insertBlock = ir.getInsertBlock();
                let insertFunc = insertBlock.parent;
                let defaultCase = null;
                {
                    let foroftmp_0 = n.cases;
                    let forofiter_1 = foroftmp_0[Symbol.iterator]();
                    let forofnext_2 = void 0;
                    while (!(forofnext_2 = forofiter_1.next()).done) {
                        let _case = forofnext_2.value;
                        {
                            if (!_case.test) {
                                defaultCase = _case;
                                break;
                            }
                        }
                    }
                }
                {
                    let foroftmp_3 = n.cases;
                    let forofiter_4 = foroftmp_3[Symbol.iterator]();
                    let forofnext_5 = void 0;
                    while (!(forofnext_5 = forofiter_4.next()).done) {
                        let _case = forofnext_5.value;
                        {
                            _case.bb = new llvm.BasicBlock('case_bb', insertFunc);
                            if (_case !== defaultCase)
                                _case.dest_check = new llvm.BasicBlock('case_dest_check_bb', insertFunc);
                        }
                    }
                }
                let merge_bb = new llvm.BasicBlock('switch_merge', insertFunc);
                let discr = _this_0.visit(n.discriminant);
                let case_checks = [];
                {
                    let foroftmp_6 = n.cases;
                    let forofiter_7 = foroftmp_6[Symbol.iterator]();
                    let forofnext_8 = void 0;
                    while (!(forofnext_8 = forofiter_7.next()).done) {
                        let _case = forofnext_8.value;
                        {
                            if (defaultCase !== _case)
                                case_checks.push({
                                    test: _case.test,
                                    dest_check: _case.dest_check,
                                    body: _case.bb
                                });
                        }
                    }
                }
                case_checks.push({ dest_check: defaultCase ? defaultCase.bb : merge_bb });
                _this_0.doInsideExitableScope(new SwitchExitableScope(merge_bb), function () {
                    let loop_casenum_1 = void 0, loop_casenum_0 = void 0;
                    ir.createBr(case_checks[0].dest_check);
                    ir.setInsertPoint(case_checks[0].dest_check);
                    for (loop_casenum_0 = 0; loop_casenum_0 < case_checks.length - 1; loop_casenum_0++) {
                        let casenum = loop_casenum_0;
                        try {
                            let test = _this_0.visit(case_checks[casenum].test);
                            let eqop = _this_0.ejs_binops['==='];
                            let discTest = _this_0.createCall(eqop, [
                                    discr,
                                    test
                                ], 'test', !eqop.doesNotThrow);
                            let disc_cmp, disc_truthy;
                            if (discTest._ejs_returns_ejsval_bool) {
                                disc_cmp = _this_0.createEjsvalICmpEq(discTest, consts.ejsval_false());
                            } else {
                                disc_truthy = _this_0.createCall(_this_0.ejs_runtime.truthy, [discTest], 'disc_truthy');
                                disc_cmp = ir.createICmpEq(disc_truthy, consts.False(), 'disccmpresult');
                            }
                            ir.createCondBr(disc_cmp, case_checks[casenum + 1].dest_check, case_checks[casenum].body);
                            ir.setInsertPoint(case_checks[casenum + 1].dest_check);
                        } finally {
                            loop_casenum_0 = casenum;
                        }
                    }
                    let case_bodies = [];
                    {
                        let foroftmp_9 = n.cases;
                        let forofiter_10 = foroftmp_9[Symbol.iterator]();
                        let forofnext_11 = void 0;
                        while (!(forofnext_11 = forofiter_10.next()).done) {
                            let _case = forofnext_11.value;
                            case_bodies.push({
                                bb: _case.bb,
                                consequent: _case.consequent
                            });
                        }
                    }
                    case_bodies.push({ bb: merge_bb });
                    for (loop_casenum_1 = 0; loop_casenum_1 < case_bodies.length - 1; loop_casenum_1++) {
                        let casenum = loop_casenum_1;
                        try {
                            ir.setInsertPoint(case_bodies[casenum].bb);
                            case_bodies[casenum].consequent.forEach(function (consequent, i) {
                                let consequent = argPresent(1) ? getArg(0) : void 0;
                                let i = argPresent(2) ? getArg(1) : void 0;
                                _this_0.visit(consequent);
                            });
                            ir.createBr(case_bodies[casenum + 1].bb);
                        } finally {
                            loop_casenum_1 = casenum;
                        }
                    }
                    ir.setInsertPoint(merge_bb);
                });
                return merge_bb;
            };
            return LLVMIRVisitor;
        }();

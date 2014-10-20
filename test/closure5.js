let ir = llvm.IRBuilder;

class LLVMIRVisitor {
    visitSwitch (n) {
        let insertBlock = ir.getInsertBlock();
        let insertFunc = insertBlock.parent;

        // find the default: case first
        let defaultCase = null;
        for (let _case of n.cases) {
            if (!_case.test) {
                defaultCase = _case;
                break;
            }
        }

        // for each case, create 2 basic blocks
        for (let _case of n.cases) {
            _case.bb = new llvm.BasicBlock("case_bb", insertFunc);
            if (_case !== defaultCase)
                _case.dest_check = new llvm.BasicBlock("case_dest_check_bb", insertFunc);
        }

        let merge_bb = new llvm.BasicBlock("switch_merge", insertFunc);

        let discr = this.visit(n.discriminant);

        let case_checks = [];
        for (let _case of n.cases) {
            if (defaultCase !== _case)
                case_checks.push ({ test: _case.test, dest_check: _case.dest_check, body: _case.bb });
        }

        case_checks.push ({ dest_check: defaultCase ? defaultCase.bb : merge_bb });

        this.doInsideExitableScope (new SwitchExitableScope(merge_bb), () => {

            // insert all the code for the tests
            ir.createBr(case_checks[0].dest_check);
            ir.setInsertPoint(case_checks[0].dest_check);
            for (let casenum = 0; casenum < case_checks.length -1; casenum ++) {
                let test = this.visit(case_checks[casenum].test);
                let eqop = this.ejs_binops["==="];
                let discTest = this.createCall(eqop, [discr, test], "test", !eqop.doesNotThrow);
                
                let disc_cmp, disc_truthy;

                if (discTest._ejs_returns_ejsval_bool) {
                    disc_cmp = this.createEjsvalICmpEq(discTest, consts.ejsval_false());
                }
                else {
                    disc_truthy = this.createCall(this.ejs_runtime.truthy, [discTest], "disc_truthy");
                    disc_cmp = ir.createICmpEq(disc_truthy, consts.False(), "disccmpresult");
                }
                ir.createCondBr(disc_cmp, case_checks[casenum+1].dest_check, case_checks[casenum].body);
                ir.setInsertPoint(case_checks[casenum+1].dest_check);
            }


            let case_bodies = [];
            
            // now insert all the code for the case consequents
            for (let _case of n.cases)
                case_bodies.push ({bb:_case.bb, consequent:_case.consequent});

            case_bodies.push ({bb:merge_bb});

            for (let casenum = 0; casenum < case_bodies.length-1; casenum ++) {
                ir.setInsertPoint(case_bodies[casenum].bb);
                case_bodies[casenum].consequent.forEach ((consequent, i) => {
                    this.visit(consequent);
                });
                
                ir.createBr(case_bodies[casenum+1].bb);
            }
            
            ir.setInsertPoint(merge_bb);

        });

        return merge_bb;
    }
}

"use strict";
var __decorate = (this && this.__decorate) || function (decorators, target, key, desc) {
    var c = arguments.length, r = c < 3 ? target : desc === null ? desc = Object.getOwnPropertyDescriptor(target, key) : desc, d;
    if (typeof Reflect === "object" && typeof Reflect.decorate === "function") r = Reflect.decorate(decorators, target, key, desc);
    else for (var i = decorators.length - 1; i >= 0; i--) if (d = decorators[i]) r = (c < 3 ? d(r) : c > 3 ? d(target, key, r) : d(target, key)) || r;
    return c > 3 && r && Object.defineProperty(target, key, r), r;
};
var __metadata = (this && this.__metadata) || function (k, v) {
    if (typeof Reflect === "object" && typeof Reflect.metadata === "function") return Reflect.metadata(k, v);
};
Object.defineProperty(exports, "__esModule", { value: true });
var core_1 = require("@angular/core");
var http_1 = require("@angular/http");
var router_1 = require("@angular/router");
var pub_class_1 = require("../pub.class");
var PDetailComponent = (function () {
    function PDetailComponent(flowService, route, router, http) {
        this.flowService = flowService;
        this.route = route;
        this.router = router;
        this.http = http;
        this.selectPolicy = new pub_class_1.Policy();
    }
    PDetailComponent.prototype.ngOnInit = function () {
        var _this = this;
        if (this.route.snapshot.params.pid == undefined) {
            console.log("pid is null");
            return;
        }
        this.pageIndex = 0;
        this.route.params
            .switchMap(function (params) { return _this.flowService.getPolicy(+params['pid']); })
            .subscribe(function (selectPolicy) {
            if (_this.route.snapshot.params.pid == 0) {
                _this.selectPolicy.id = 0;
                _this.selectPolicy.type = null;
                _this.selectPolicy.gid = _this.route.snapshot.params.gid;
            }
            else {
                _this.selectPolicy = selectPolicy;
            }
            _this.refreshPage(_this.selectPolicy);
            $('#exampleModal').modal('show');
            $("#exampleAlert").alert("close");
        });
    };
    PDetailComponent.prototype.onFirstPage = function (policy) {
        this.pageIndex = 0;
        this.refreshPage(policy);
        return false;
    };
    PDetailComponent.prototype.onPreviousPage = function (policy) {
        this.pageIndex--;
        this.refreshPage(policy);
        return false;
    };
    PDetailComponent.prototype.onNextPage = function (policy) {
        this.pageIndex++;
        this.refreshPage(policy);
        return false;
    };
    PDetailComponent.prototype.refreshPage = function (policy) {
        var _this = this;
        this.policyDatas = this.flowService.getPolicyDatas(this.pageIndex, policy.id).then(function (res) {
            if (res instanceof Array) {
                _this.pageLen = res.length;
                if (_this.pageLen > 15)
                    res.pop();
            }
            return res;
        });
    };
    PDetailComponent.prototype.onSavePolicy = function () {
        var _this = this;
        if (!this.selectPolicy.type) {
            alert("'type' is null");
            return false;
        }
        if (window.confirm("确认要操作吗？")) {
            this.http.post('/admin/action/policy/set', JSON.stringify(this.selectPolicy)).toPromise().then(function (response) {
                //do something...
                if (response.status == 200) {
                    //alert('The Policy save successful!');
                    //$(jQuery).bootstrapGrowl("The Policy save successful!");
                    //setTimeout(function(){$("#exampleAlert").alert("close");},2000);
                    var insert_id;
                    insert_id = JSON.parse(response.text()).insert_id;
                    if (insert_id != null) {
                        _this.selectPolicy.id = insert_id;
                    }
                    _this.refreshPage(_this.selectPolicy);
                }
            });
        }
    };
    PDetailComponent.prototype.onEditPD = function (pd, ptype) {
        this.router.navigate(["pd", ptype, pd.pid, pd.id, this.flowService.getNextSeq()], { relativeTo: this.route });
        return false;
    };
    PDetailComponent.prototype.onDeletePD = function (pd) {
        var _this = this;
        if (window.confirm("确认要删除吗？")) {
            this.http.delete('/admin/action/policy/data/del?id=' + pd.id + "&ptype=" + this.selectPolicy.type).toPromise().then(function (response) {
                if (response.status == 200) {
                    _this.refreshPage(_this.selectPolicy);
                }
            });
        }
        return false;
    };
    PDetailComponent.prototype.onSavePD = function () {
        var _this = this;
        if (!window.confirm("确认要操作吗？"))
            return false;
        var pd = this.flowService.getLastPolicyData();
        var datas = new Array();
        datas.push(pd);
        this.http.post('/admin/action/policy/data/set?ptype=' + this.selectPolicy.type, JSON.stringify(datas)).toPromise().then(function (response) {
            //do something...
            if (response.status == 200) {
                //alert('The Policy Data saved successful!');
                _this.refreshPage(_this.selectPolicy);
            }
        });
    };
    PDetailComponent.prototype.onAddPD = function () {
        this.router.navigate(["pd", this.selectPolicy.type, this.selectPolicy.id, 0, this.flowService.getNextSeq()], { relativeTo: this.route });
    };
    PDetailComponent.prototype.onAddPDCID = function () {
        this.router.navigate(["pd", this.selectPolicy.type, this.selectPolicy.id, 0, this.flowService.getNextSeq()], { relativeTo: this.route });
    };
    PDetailComponent.prototype.onAddPDAID = function () {
        this.router.navigate(["pd", this.selectPolicy.type, this.selectPolicy.id, 0, this.flowService.getNextSeq()], { relativeTo: this.route });
    };
    return PDetailComponent;
}());
PDetailComponent = __decorate([
    core_1.Component({
        moduleId: module.id,
        templateUrl: '../../../templates/p-detail.template.html'
    }),
    __metadata("design:paramtypes", [pub_class_1.DivsFlowService,
        router_1.ActivatedRoute,
        router_1.Router, http_1.Http])
], PDetailComponent);
exports.PDetailComponent = PDetailComponent;
var PDDetailComponent = (function () {
    function PDDetailComponent(flowService, route, router) {
        this.flowService = flowService;
        this.route = route;
        this.router = router;
    }
    PDDetailComponent.prototype.ngOnInit = function () {
        var _this = this;
        if (this.route.snapshot.params.id == undefined) {
            console.log("pdid is null");
            return;
        }
        this.route.params
            .switchMap(function (params) { return _this.flowService.getPolicyData(+params['id']); })
            .subscribe(function (policyData) {
            _this.ptype = _this.route.snapshot.params.ptype;
            _this.policyData = policyData;
            if (_this.route.snapshot.params.id == 0) {
                policyData.pid = _this.route.snapshot.params.pid;
            }
        });
    };
    return PDDetailComponent;
}());
PDDetailComponent = __decorate([
    core_1.Component({
        moduleId: module.id,
        selector: 'crisis-center',
        template: "\n    <table class=\"table\" *ngIf=\"policyData\">        \n        <tr *ngIf=\"ptype=='aidrange' || ptype=='cidrange' || ptype=='iprange'\">\n            <td>pdid:</td>\n            <td>{{policyData.id}}</td>\n            <td>start:</td>\n            <td><input type=\"number\" name=\"start\" class=\"form-control\" [(ngModel)]=\"policyData.start\"></td>\n            <td>end:</td>\n            <td><input type=\"number\" name=\"start\" class=\"form-control\" [(ngModel)]=\"policyData.end\"></td>\n        </tr>\n        <tr *ngIf=\"ptype=='aidset' || ptype=='aidsuffix' || ptype=='cidset' || ptype=='cidsuffix' || ptype=='oidset' || ptype=='ipset'\">\n            <td>pdid:</td>\n            <td>{{policyData.id}}</td>\n            <td>val:</td>\n            <td><input type=\"number\" name=\"val\" class=\"form-control\" [(ngModel)]=\"policyData.val\"></td>\n        </tr>\n    </table>\n"
    }),
    __metadata("design:paramtypes", [pub_class_1.DivsFlowService,
        router_1.ActivatedRoute,
        router_1.Router])
], PDDetailComponent);
exports.PDDetailComponent = PDDetailComponent;
var CidsetComponent = (function () {
    function CidsetComponent(flowService, route, router) {
        this.flowService = flowService;
        this.route = route;
        this.router = router;
    }
    CidsetComponent.prototype.ngOnInit = function () {
        var _this = this;
        if (this.route.snapshot.params.id == undefined) {
            console.log("pdid is null");
            return;
        }
        this.route.params
            .switchMap(function (params) { return _this.flowService.getPolicyData(+params['id']); })
            .subscribe(function (policyData) {
            _this.ptype = _this.route.snapshot.params.ptype;
            _this.policyData = policyData;
            if (_this.route.snapshot.params.id == 0) {
                policyData.pid = _this.route.snapshot.params.pid;
            }
        });
    };
    return CidsetComponent;
}());
CidsetComponent = __decorate([
    core_1.Component({
        moduleId: module.id,
        selector: 'crisis-center',
        template: "\n    no implement\n"
    }),
    __metadata("design:paramtypes", [pub_class_1.DivsFlowService,
        router_1.ActivatedRoute,
        router_1.Router])
], CidsetComponent);
exports.CidsetComponent = CidsetComponent;
//# sourceMappingURL=p-detail.component.js.map
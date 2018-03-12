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
var router_1 = require("@angular/router");
var http_1 = require("@angular/http");
var pub_class_1 = require("../pub.class");
var FlowComponent = (function () {
    //policyGroupPromise:Promise<PolicyGroup>;
    function FlowComponent(router, route, http, flowService) {
        var _this = this;
        this.router = router;
        this.route = route;
        this.http = http;
        this.flowService = flowService;
        this.selectPolicyGroup = new pub_class_1.PolicyGroup();
        route.params.subscribe(function (_) { return _this.id = _.id; });
    }
    FlowComponent.prototype.ngOnInit = function () {
        var _this = this;
        var eid = this.route.snapshot.params.id;
        if (eid == undefined) {
            console.log("fid is null");
            return;
        }
        this.route.params.subscribe(function (data) {
            eid = data['id'];
            if (eid == undefined) {
                console.log("fid is null");
                return;
            }
            console.log('queryParams', data['id']);
            _this.getFlow(eid).then(function (flows) {
                if (flows.length == 0) {
                    _this.flow.fid = 0;
                    _this.flow.title = null;
                    _this.flow.status = null;
                }
                return flows.find(function (f) {
                    _this.flow = f;
                    _this.policyGroups = _this.flowService.getPolicyGroups(f.fid);
                    return true;
                });
            });
        });
        console.log(this.policyGroups);
    };
    FlowComponent.prototype.getFlow = function (eid) {
        //if ( ! this.flowsPromise)
        return this.http.get('/admin/action/flow/get?eid=' + eid).toPromise().then(function (res) {
            var tmpflows;
            tmpflows = res.json();
            for (var i = 0; i < tmpflows.length; i++) {
                if (tmpflows[i].fid == null)
                    tmpflows[i].fid = 0;
            }
            return tmpflows;
        });
    };
    FlowComponent.prototype.ngOnDestroy = function () {
        // prevent memory leak when component destroyed
        //this.subscription.unsubscribe();
        console.log("ngOnDestroy");
    };
    FlowComponent.prototype.ngDoCheck = function () {
        //console.log(this.route.queryParams.take.name);
    };
    FlowComponent.prototype.refushFlow = function () {
        var _this = this;
        this.getFlow(this.flow.eid).then(function (flows) { return flows.find(function (f) {
            _this.flow = f;
            return true;
        }); });
    };
    FlowComponent.prototype.onSyncFlow = function (fid) {
        var _this = this;
        if (window.confirm("确认要操作吗？")) {
            this.http.put('/admin/action/flow/sync?fid=' + fid, null).toPromise().then(function (response) {
                if (response.status == 200) {
                    _this.refushFlow();
                }
            });
        }
    };
    FlowComponent.prototype.onSaveFlow = function () {
        var _this = this;
        //this.router.navigate(['/flow',this.flow.fid,"1"], { queryParams:{fid:this.flow.fid},relativeTo:this.route});
        if (this.flow.title == null || this.flow.status == null) {
            console.log(this.flow);
            alert("The Flow can't null");
            return false;
        }
        if (window.confirm("确认要操作吗？")) {
            this.flow.status = Number(this.flow.status);
            //let options = new RequestOptions({ headers: headers });
            this.http.post('/admin/action/flow/set', JSON.stringify(this.flow)).toPromise().then(function (response) {
                //do something...
                if (response.status == 200) {
                    _this.refushFlow();
                }
            });
        }
    };
    FlowComponent.prototype.addPolicyGroup = function () {
        //this.selectPolicyGroup=new PolicyGroup();
        this.router.navigate(["pg", 0, this.flow.fid], { relativeTo: this.route });
    };
    FlowComponent.prototype.onEditPG = function (policyGroup) {
        //this.policyGroupPromise = Promise.resolve(policyGroup);
        this.selectPolicyGroup = policyGroup;
        this.router.navigate(["pg", policyGroup.gid, this.flow.fid], { relativeTo: this.route });
        //this.router.navigateByUrl("/flow/"+this.flow.fid+"/"+policyGroup.gid);
        console.log(policyGroup);
        //this.router.navigate([{outlets: {page2: ['/flow',this.flow.fid,policyGroup.gid]}}]).then(_ => { });
        return false;
    };
    FlowComponent.prototype.onDeletePG = function (pg) {
        var _this = this;
        if (window.confirm("确认要操作吗？")) {
            this.http.delete('/admin/action/group/pg?gid=' + pg.gid).toPromise().then(function (response) {
                //do something...
                if (response.status == 200) {
                    //alert('The PolicyGroup delete successful!');
                    _this.policyGroups = _this.flowService.getPolicyGroups(_this.flow.fid);
                }
            });
        }
        return false;
    };
    FlowComponent.prototype.onAddPolicy = function (policyGroup) {
        console.log(policyGroup);
        this.selectPolicyGroup = policyGroup;
        this.router.navigate(["p", policyGroup.gid, 0, this.flowService.getNextSeq()], { relativeTo: this.route });
        return false;
    };
    FlowComponent.prototype.onEditPolicy = function (gid, policy) {
        console.log(policy);
        this.router.navigate(["p", gid, policy.id, this.flowService.getNextSeq()], { relativeTo: this.route });
        return false;
    };
    FlowComponent.prototype.onDeletePolicy = function (p) {
        var _this = this;
        if (window.confirm("确认要操作吗？")) {
            this.http.delete('/admin/action/policy/del?pid=' + p.id).toPromise().then(function (response) {
                //do something...
                if (response.status == 200) {
                    //alert('The Policy delete successful!');
                    //this.getPolicys(p.id);
                    _this.policyGroups = _this.flowService.getPolicyGroups(_this.flow.fid);
                }
            });
        }
        return false;
    };
    FlowComponent.prototype.openPopup = function (policyGroup) {
        this.router.navigate([{ outlets: { popup: ['message', policyGroup.gid] } }]).then(function (_) {
            // navigation is done
        });
    };
    return FlowComponent;
}());
FlowComponent = __decorate([
    core_1.Component({
        moduleId: module.id,
        //selector: 'flow-main',
        //selector: 'my-app',
        templateUrl: '../../../templates/main.template.html'
    }),
    __metadata("design:paramtypes", [router_1.Router, router_1.ActivatedRoute, http_1.Http, pub_class_1.DivsFlowService])
], FlowComponent);
exports.FlowComponent = FlowComponent;
//# sourceMappingURL=flow.component.js.map
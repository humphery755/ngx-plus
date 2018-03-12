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
var PGDetailComponent = (function () {
    function PGDetailComponent(flowService, route, router, http) {
        this.flowService = flowService;
        this.route = route;
        this.router = router;
        this.http = http;
        this.selectPolicyGroup = new pub_class_1.PolicyGroup();
    }
    PGDetailComponent.prototype.ngOnInit = function () {
        var _this = this;
        if (this.route.snapshot.params.id == undefined) {
            console.log("gid is null");
            return;
        }
        this.route.params
            .switchMap(function (params) { return _this.flowService.getPolicyGroup(+params['id']); })
            .subscribe(function (selectPolicyGroup) {
            //this.editName = crisis.name;
            //console.log(this.route.snapshot.params.id);
            if (_this.route.snapshot.params.id == 0) {
                _this.selectPolicyGroup = new pub_class_1.PolicyGroup();
                _this.selectPolicyGroup.action = 1;
                _this.selectPolicyGroup.uri = "/";
                _this.selectPolicyGroup.level = 1;
                _this.selectPolicyGroup.fid = _this.route.snapshot.params.fid;
                _this.selectPolicyGroup.gid = 0;
            }
            else
                _this.selectPolicyGroup = selectPolicyGroup;
        });
    };
    PGDetailComponent.prototype.onSavePolicyGroup = function (policyGroup) {
        var _this = this;
        this.http.post('/admin/action/group/set', JSON.stringify(this.selectPolicyGroup)).toPromise().then(function (response) {
            if (response.status == 200) {
                var eid = _this.route.parent.snapshot.params.id;
                _this.router.navigate(['/admin/flow', eid, _this.flowService.getNextSeq()], { relativeTo: _this.route.parent.parent });
            }
        });
    };
    return PGDetailComponent;
}());
PGDetailComponent = __decorate([
    core_1.Component({
        moduleId: module.id,
        selector: 'pg-detail',
        templateUrl: '../../../templates/pg-detail.template.html'
    }),
    __metadata("design:paramtypes", [pub_class_1.DivsFlowService,
        router_1.ActivatedRoute,
        router_1.Router, http_1.Http])
], PGDetailComponent);
exports.PGDetailComponent = PGDetailComponent;
var NothingComponent = (function () {
    function NothingComponent() {
    }
    NothingComponent.prototype.ngOnInit = function () {
    };
    return NothingComponent;
}());
NothingComponent = __decorate([
    core_1.Component({
        moduleId: module.id,
        selector: 'crisis-center',
        template: "<h2>\u5371\u673A\u4E2D\u5FC3</h2>\n<router-outlet></router-outlet>"
    }),
    __metadata("design:paramtypes", [])
], NothingComponent);
exports.NothingComponent = NothingComponent;
//# sourceMappingURL=pg-detail.component.js.map
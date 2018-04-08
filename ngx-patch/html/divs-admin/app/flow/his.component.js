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
var HisFlowComponent = (function () {
    //policyGroupPromise:Promise<PolicyGroup>;
    function HisFlowComponent(router, route, http, flowService) {
        var _this = this;
        this.router = router;
        this.route = route;
        this.http = http;
        this.flowService = flowService;
        route.params.subscribe(function (_) { return _this.id = _.id; });
    }
    HisFlowComponent.prototype.ngOnInit = function () {
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
            _this.getFlow(eid).then(function (flows) {
                _this.flows = flows;
            });
        });
    };
    HisFlowComponent.prototype.getFlow = function (eid) {
        return this.http.get('/admin/action/flow/get_his?eid=' + eid).toPromise().then(function (res) {
            var tmp;
            tmp = res.json();
            if (tmp instanceof Array)
                return tmp;
            return null;
        });
    };
    return HisFlowComponent;
}());
HisFlowComponent = __decorate([
    core_1.Component({
        moduleId: module.id,
        //selector: 'flow-main',
        //selector: 'my-app',
        templateUrl: '../../../templates/hisflows.template.html'
    }),
    __metadata("design:paramtypes", [router_1.Router, router_1.ActivatedRoute, http_1.Http, pub_class_1.DivsFlowService])
], HisFlowComponent);
exports.HisFlowComponent = HisFlowComponent;
//# sourceMappingURL=his.component.js.map
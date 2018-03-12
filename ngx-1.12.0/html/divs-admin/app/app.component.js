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
require("rxjs/add/operator/switchMap");
var pub_class_1 = require("./pub.class");
//export const ROUTER_DIRECTIVES = [RouterOutlet, RouterLink, RouterLinkWithHref, RouterLinkActive];
var AppComponent = (function () {
    function AppComponent(router, route, userService) {
        this.router = router;
        this.route = route;
        this.userService = userService;
        this.title = '英雄帖';
    }
    AppComponent.prototype.ngOnInit = function () {
        var _this = this;
        this.userService.populate();
        this.userService.isAuthenticated.subscribe(function (isAuthenticated) {
            console.log(isAuthenticated);
            _this.isSingle = !isAuthenticated;
            if (isAuthenticated)
                _this.router.navigateByUrl('/admin');
            else
                _this.router.navigateByUrl('/login');
        });
    };
    return AppComponent;
}());
AppComponent = __decorate([
    core_1.Component({
        moduleId: module.id,
        selector: 'app-root',
        templateUrl: 'app.component.html',
        styleUrls: []
    }),
    __metadata("design:paramtypes", [router_1.Router, router_1.ActivatedRoute,
        pub_class_1.UserService])
], AppComponent);
exports.AppComponent = AppComponent;
var DivsAdminComponent = (function () {
    function DivsAdminComponent(router, route, flowService) {
        this.router = router;
        this.route = route;
        this.flowService = flowService;
        // this.mainPage = new MainPageComponent();
    }
    DivsAdminComponent.prototype.ngOnInit = function () {
        var _this = this;
        //this.flows = this.flowService.getFlows();
        this.envTrees = this.route.params
            .switchMap(function (params) {
            return _this.flowService.getEnvTree();
        });
    };
    DivsAdminComponent.prototype.onSelect = function (env) {
        this.selectedEnv = env;
        if (env != null) {
            //console.log(env);
            this.router.navigate(['flow', env.eid, this.flowService.getNextSeq()], { relativeTo: this.route });
            //this.router.navigate([{outlets: {root_routlet: ['flow', flow.fid]}}]).then(_ => {});
            //this.router.navigate(['/crisis-center', {fid:flow.fid}]); this.router.navigate(['/flow', {fid:flow.fid}],{ queryParams: { fid:flow.fid} });
        }
        return false;
        //console.log(this.fcService.change);
        //console.log(flow);
    };
    DivsAdminComponent.prototype.onListHisFlows = function (env) {
        if (env != null) {
            this.router.navigate(['his/flow', env.eid, this.flowService.getNextSeq()], { relativeTo: this.route });
        }
        return false;
    };
    DivsAdminComponent.prototype.onListNodeInfo = function () {
        console.log("monitor/nodes");
        this.router.navigate(['monitor/nodes', this.flowService.getNextSeq()], { relativeTo: this.route });
        //this.router.navigateByUrl('/monitor/nodes/'+this.flowService.getNextSeq());
        return false;
    };
    return DivsAdminComponent;
}());
DivsAdminComponent = __decorate([
    core_1.Component({
        moduleId: module.id,
        selector: 'app-root',
        //directives: [ROUTER_DIRECTIVES],
        templateUrl: './../templates/divs.admin.html'
    }),
    __metadata("design:paramtypes", [router_1.Router, router_1.ActivatedRoute, pub_class_1.DivsFlowService])
], DivsAdminComponent);
exports.DivsAdminComponent = DivsAdminComponent;
//# sourceMappingURL=app.component.js.map
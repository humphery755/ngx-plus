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
var NodeComponent = (function () {
    function NodeComponent(router, route, http) {
        var _this = this;
        this.router = router;
        this.route = route;
        this.http = http;
        route.params.subscribe(function (_) { return _this.id = _.id; });
    }
    NodeComponent.prototype.ngOnInit = function () {
        var _this = this;
        this.route.params.subscribe(function (data) {
            _this.getNodes().then(function (nodes) {
                _this.nodes = nodes;
            });
        });
    };
    NodeComponent.prototype.getNodes = function () {
        return this.http.get('/admin/action/monitor/get_node_info').toPromise().then(function (res) { return res.json(); });
    };
    return NodeComponent;
}());
NodeComponent = __decorate([
    core_1.Component({
        moduleId: module.id,
        //selector: 'flow-main',
        //selector: 'my-app',
        templateUrl: '../../../templates/nodes.template.html'
    }),
    __metadata("design:paramtypes", [router_1.Router, router_1.ActivatedRoute, http_1.Http])
], NodeComponent);
exports.NodeComponent = NodeComponent;
//# sourceMappingURL=node.component.js.map
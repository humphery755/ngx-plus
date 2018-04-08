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
/**
 * Created by yitala on 2016/12/6.
 */
var core_1 = require("@angular/core");
var router_1 = require("@angular/router");
var pub_class_1 = require("../pub.class");
var NeedAuthGuard = (function () {
    function NeedAuthGuard(router, userService) {
        this.router = router;
        this.userService = userService;
    }
    NeedAuthGuard.prototype.canActivate = function (route, state) {
        var _this = this;
        console.log(this.userService.isAuthenticated);
        this.userService.isAuthenticated.subscribe(function (isAuthenticated) {
            if (!isAuthenticated) {
                _this.router.navigate(["/login"]);
            }
            _this.result = isAuthenticated;
        });
        return this.result;
    };
    return NeedAuthGuard;
}());
NeedAuthGuard = __decorate([
    core_1.Injectable(),
    __metadata("design:paramtypes", [router_1.Router,
        pub_class_1.UserService])
], NeedAuthGuard);
exports.NeedAuthGuard = NeedAuthGuard;
//# sourceMappingURL=no-auth-guard.service.js.map
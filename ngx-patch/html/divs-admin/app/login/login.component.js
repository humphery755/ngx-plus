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
var http_1 = require("@angular/http");
var LoginComponent = (function () {
    function LoginComponent(http, router, userService) {
        this.http = http;
        this.router = router;
        this.userService = userService;
        this.model = { username: '', password: '' };
    }
    LoginComponent.prototype.login = function () {
        var _this = this;
        var user = this.model;
        var body = 'username=' + user.username + '&passwd=' + user.password;
        this.http.put('/admin/action/login/user', body).toPromise().then(function (res) {
            var tmp = res.json();
            if (tmp.code == 0) {
                user.token = tmp.token;
                _this.userService.setAuth(user);
                _this.router.navigateByUrl('/admin');
            }
            else {
                _this.error = '用户名或密码错误';
            }
            return null;
        });
    };
    return LoginComponent;
}());
LoginComponent = __decorate([
    core_1.Component({
        moduleId: module.id,
        selector: 'app-root',
        templateUrl: '../../templates/login.component.html',
        styleUrls: ['../../templates/login.component.css']
    }),
    __metadata("design:paramtypes", [http_1.Http,
        router_1.Router,
        pub_class_1.UserService])
], LoginComponent);
exports.LoginComponent = LoginComponent;
//# sourceMappingURL=login.component.js.map
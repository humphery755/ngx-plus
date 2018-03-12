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
require("rxjs/Rx");
var ReplaySubject_1 = require("rxjs/ReplaySubject");
var headers = new Headers({ 'Content-Type': 'application/json' });
var DivsEnv = (function () {
    function DivsEnv() {
    }
    return DivsEnv;
}());
exports.DivsEnv = DivsEnv;
var DivsFlow = (function () {
    function DivsFlow() {
    }
    return DivsFlow;
}());
exports.DivsFlow = DivsFlow;
var PolicyData = (function () {
    function PolicyData() {
    }
    return PolicyData;
}());
exports.PolicyData = PolicyData;
var Policy = (function () {
    function Policy() {
    }
    return Policy;
}());
exports.Policy = Policy;
var PolicyGroup = (function () {
    function PolicyGroup() {
    }
    return PolicyGroup;
}());
exports.PolicyGroup = PolicyGroup;
var PolicyGroupWrap = (function () {
    function PolicyGroupWrap() {
    }
    return PolicyGroupWrap;
}());
exports.PolicyGroupWrap = PolicyGroupWrap;
var DivsFlowService = (function () {
    function DivsFlowService(http) {
        this.http = http;
        this.sequence = 1;
        this.lastPolicyData = new PolicyData();
        console.log(" AppComponent constructor :", "run step constructor ");
    }
    DivsFlowService.prototype.getNextSeq = function () {
        return ++this.sequence;
    };
    DivsFlowService.prototype.getEnvTree = function () {
        //if ( ! this.flowsPromise)
        return this.http.get('/admin/action/flow/getenv').toPromise().then(function (res) {
            var tmpflows;
            tmpflows = res.json();
            for (var i = 0; i < tmpflows.length; i++) {
                if (tmpflows[i].eid == null)
                    tmpflows[i].eid = 0;
            }
            return tmpflows;
        });
    };
    /*
    getFlow(eid:number){
      //if ( ! this.flowsPromise)
      this.flowsPromise = this.http.get('/admin/action/flow/get?eid='+eid).toPromise().then(res => {
          var tmpflows:DivsFlow[];
          tmpflows = res.json() as DivsFlow[];
          for(var i=0;i<tmpflows.length;i++){
            if(tmpflows[i].fid == null)tmpflows[i].fid=0;
          }
          return tmpflows;
        });
      console.log(this.flowsPromise);
      return this.flowsPromise;
    }
    getFlow(fid:number) {
      var f:DivsFlow;
      this.flowsPromise.then(flows => flows.find(flow => flow.fid == fid));
      console.log(f);
      return f;
    }
    
    getDefaultSelectedFlow() {
      return this.flowsPromise.then(flows => flows.find(flow => flow.fid > 0));
    }
  
    
  
    setSelectedFlow(flow:DivsFlow){
      this.selectFlow=flow;
    }
    getSelectedFlow():DivsFlow{
      return this.selectFlow;
    }
  */
    DivsFlowService.prototype.getPolicyGroups = function (fid) {
        if (!fid) {
            return null;
        } //return Promise.resolve([]);
        //console.log(fid);
        this.policyGroupsPromise = this.http.get('/admin/action/group/get?fid=' + fid).toPromise().then(function (res) {
            var tmp;
            tmp = res.json();
            if (tmp instanceof Array) {
                return tmp;
            }
            console.log(tmp);
            return null;
        });
        return this.policyGroupsPromise;
    };
    DivsFlowService.prototype.getPolicyGroup = function (gid) {
        if (this.policyGroupsPromise == null)
            return Promise.resolve(null);
        return this.policyGroupsPromise.then(function (pgs) {
            if (pgs == null)
                return null;
            if (pgs instanceof Array && pgs.length > 0)
                return pgs.find(function (policyGroup) { return (policyGroup) && policyGroup.gid == gid; });
            else
                return pgs;
        });
        //return flows.find(flows => flows.fid === fid);
    };
    DivsFlowService.prototype.getPolicy = function (pid) {
        this.policysPromise = this.http.get('/admin/action/policy/get?pid=' + pid).toPromise().then(function (res) {
            var tmp;
            tmp = res.json();
            if (tmp instanceof Array) {
                return tmp.find(function (policy) { return (policy) && policy.id == pid; });
            }
            return tmp;
        });
        return this.policysPromise;
    };
    DivsFlowService.prototype.getPolicyDatas = function (index, pid) {
        this.policyDatasPromise = this.http.get('/admin/action/policy/data/get?pid=' + pid + '&pageIndex=' + index).toPromise().then(function (res) {
            var tmp;
            tmp = res.json();
            if (tmp instanceof Array) {
                return tmp;
            }
            console.log(tmp);
            return null;
        });
        return this.policyDatasPromise;
    };
    DivsFlowService.prototype.getPolicyData = function (id) {
        var _this = this;
        return this.policyDatasPromise.then(function (res) {
            var tmp;
            if (res instanceof Array) {
                tmp = res.find(function (pd) { return (pd) && pd.id == id; });
                if (tmp != null) {
                    _this.lastPolicyData = tmp;
                    return tmp;
                }
            }
            _this.lastPolicyData.id = id;
            return _this.lastPolicyData;
        });
    };
    DivsFlowService.prototype.getLastPolicyData = function () {
        return this.lastPolicyData;
    };
    return DivsFlowService;
}());
DivsFlowService = __decorate([
    core_1.Injectable(),
    __metadata("design:paramtypes", [http_1.Http])
], DivsFlowService);
exports.DivsFlowService = DivsFlowService;
var DivsFlowChangeService = (function () {
    function DivsFlowChangeService() {
        this.change = new core_1.EventEmitter();
        // console.log(this);
    }
    return DivsFlowChangeService;
}());
__decorate([
    core_1.Output(),
    __metadata("design:type", core_1.EventEmitter)
], DivsFlowChangeService.prototype, "change", void 0);
DivsFlowChangeService = __decorate([
    core_1.Injectable(),
    __metadata("design:paramtypes", [])
], DivsFlowChangeService);
exports.DivsFlowChangeService = DivsFlowChangeService;
var User = (function () {
    function User() {
    }
    return User;
}());
exports.User = User;
var UserService = (function () {
    function UserService(http) {
        this.http = http;
        this.isAuthenticatedSubject = new ReplaySubject_1.ReplaySubject(1);
        this.isAuthenticated = this.isAuthenticatedSubject.asObservable();
        console.log(" UserService constructor :", "run step constructor ");
    }
    UserService.prototype.getCookie = function (c_name) {
        console.log(document.cookie);
        if (document.cookie.length > 0) {
            var c_start = document.cookie.indexOf(c_name + "=");
            if (c_start != -1) {
                c_start = c_start + c_name.length + 1;
                var c_end = document.cookie.indexOf(";", c_start);
                if (c_end == -1)
                    c_end = document.cookie.length;
                //return unescape(document.cookie.substring(c_start,c_end))
                return document.cookie.substring(c_start, c_end);
            }
        }
        return null;
    };
    UserService.prototype.populate = function () {
        if (this.getCookie("access_token") == null) {
            this.clearAuth();
        }
        else {
            this.isAuthenticatedSubject.next(true);
        }
        /*
          if(window.localStorage['access_token']){
              this.setAuth({username:'linweiwei',token:'token-1',password:'123'} as User)
          }
          else{
              this.clearAuth();
          }*/
    };
    UserService.prototype.setAuth = function (user) {
        // Save JWT sent from server in localstorage
        //window.localStorage['access_token'] = user.token
        // Set isAuthenticated to true
        this.isAuthenticatedSubject.next(true);
    };
    UserService.prototype.clearAuth = function () {
        // Remove JWT from localstorage
        window.localStorage.removeItem('access_token');
        // Set auth status to false
        this.isAuthenticatedSubject.next(false);
    };
    return UserService;
}());
UserService = __decorate([
    core_1.Injectable(),
    __metadata("design:paramtypes", [http_1.Http])
], UserService);
exports.UserService = UserService;
//# sourceMappingURL=pub.class.js.map
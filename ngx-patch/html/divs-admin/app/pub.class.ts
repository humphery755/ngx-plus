import { Component,Injectable,Output,EventEmitter,OnInit } from '@angular/core';
import {Http,RequestOptions} from '@angular/http';
import { CanActivate }    from '@angular/router';
import {Observable} from 'rxjs/Observable';

import 'rxjs/Rx';
import {ReplaySubject} from "rxjs/ReplaySubject";

let headers = new Headers({ 'Content-Type': 'application/json' });

export class DivsEnv {
  eid:number;
  div_env:string;
}

export class DivsFlow {
  fid:number;
  eid:number;
  title:string;
  crt_date:string;
  src:string;
  to:string;
  public div_env:string;
  ver:number;
  status:number;
}
export class PolicyData {
  id:number;
  pid:number;  
  start:number;
  end:number;
  val:string;
}
export class Policy {
  id:number;
  gid:number;
  type:string;
  //datas:any[];
}
export class PolicyGroup {
  gid:number;
  fid:number;
  action:number;
  level:number;
  uri:string;
  backend:string;
  text:string;
  policys:Policy[];
}

export class PolicyGroupWrap {
  gid:number;
  fid:number;
  action:number;
  level:number;
  uri:string;
  backend:string;
  text:string;
  pid:number;
  type:string;
}

@Injectable()
export class DivsFlowService{
  sequence:number=1;  
  flowsPromise:Promise<DivsFlow[]>;
  selectFlow:DivsFlow;
  policyGroupsPromise:Promise<PolicyGroup[]>;
  policysPromise:Promise<Policy>;
  policyDatasPromise:Promise<PolicyData[]>;

  constructor(private http: Http) {
      console.log(" AppComponent constructor :", "run step constructor ");   
  }

  getNextSeq():number{
    return ++this.sequence;
  }

  getEnvTree(){
    //if ( ! this.flowsPromise)
    return this.http.get('/admin/action/flow/getenv').toPromise().then(res => {
        var tmpflows:DivsEnv[];
        tmpflows = res.json() as DivsEnv[];
        for(var i=0;i<tmpflows.length;i++){
          if(tmpflows[i].eid == null)tmpflows[i].eid=0;
        }
        return tmpflows;
      });
  }
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

  getPolicyGroups(fid:number){
    if(! fid){
      return null;
    }//return Promise.resolve([]);
    //console.log(fid);
    this.policyGroupsPromise = this.http.get('/admin/action/group/get?fid='+fid).toPromise().then(res => {
      var tmp:PolicyGroup[];
      tmp = res.json() as PolicyGroup[];
      if(tmp instanceof Array){
        return tmp;
      }
      console.log(tmp);
      return null;
  });
    return this.policyGroupsPromise;
  }

  getPolicyGroup(gid:number) {
    if (this.policyGroupsPromise == null)return Promise.resolve(null);
    return this.policyGroupsPromise.then(pgs => {
      if(pgs==null)return null;
      if(pgs instanceof Array && pgs.length>0)
      return pgs.find(policyGroup => (policyGroup) && policyGroup.gid == gid)
      else return pgs;
    });
    //return flows.find(flows => flows.fid === fid);
  }


  getPolicy(pid:number) {

    this.policysPromise = this.http.get('/admin/action/policy/get?pid='+pid).toPromise().then(res => {
        var tmp:Policy[];
        tmp = res.json() as Policy[];
        if(tmp instanceof Array){
          return tmp.find(policy => (policy) && policy.id == pid)
        }
        return tmp;
    });
    return this.policysPromise;

  }


   getPolicyDatas(index:number,pid:number) {
    this.policyDatasPromise = this.http.get('/admin/action/policy/data/get?pid='+pid+'&pageIndex='+index).toPromise().then(res => {
        var tmp:PolicyData[];
        tmp = res.json() as PolicyData[];
        if(tmp instanceof Array){
          return tmp;
        }
        console.log(tmp);
        return null;
    });
    return this.policyDatasPromise;

  }
  lastPolicyData:PolicyData=new PolicyData();
  getPolicyData(id:number) {
    return this.policyDatasPromise.then(res => {
        var tmp:PolicyData;
        if(res instanceof Array){
          tmp = res.find(pd => (pd) && pd.id == id)
          if(tmp!=null){
            this.lastPolicyData=tmp;
            return tmp;
          }
        }
        this.lastPolicyData.id=id;
        return this.lastPolicyData;
    });
  }

  getLastPolicyData(){
    return this.lastPolicyData
  }


}

@Injectable()
export class DivsFlowChangeService {
    @Output() change: EventEmitter<any>;

    constructor(){
        this.change = new EventEmitter();
       // console.log(this);
    }
}

export class User{
    username:string;
    token:string;
    //密码不能放前端，这里暂时这么做
    password:string;
}

@Injectable()
export class UserService{

    private isAuthenticatedSubject = new ReplaySubject<boolean>(1);
    public isAuthenticated = this.isAuthenticatedSubject.asObservable();

    constructor(private http: Http) {
        console.log(" UserService constructor :", "run step constructor ");   
    }
    
    getCookie(c_name:string)
    {
      console.log(document.cookie);
      if (document.cookie.length>0) {
        var c_start=document.cookie.indexOf(c_name + "=")
        if (c_start!=-1)
          { 
          c_start=c_start + c_name.length+1 
          var c_end=document.cookie.indexOf(";",c_start)
          if (c_end==-1) c_end=document.cookie.length
          //return unescape(document.cookie.substring(c_start,c_end))
          return document.cookie.substring(c_start,c_end);
          } 
        }
      return null;
    }
    populate(){
      if(this.getCookie("access_token")==null){
        this.clearAuth();
      }else{
        this.isAuthenticatedSubject.next(true);
      }
      /*
        if(window.localStorage['access_token']){
            this.setAuth({username:'linweiwei',token:'token-1',password:'123'} as User)
        }
        else{
            this.clearAuth();
        }*/
        
    }

    setAuth(user: User) {
        // Save JWT sent from server in localstorage
        //window.localStorage['access_token'] = user.token
        // Set isAuthenticated to true
        this.isAuthenticatedSubject.next(true);
    }

    clearAuth() {
        // Remove JWT from localstorage
        window.localStorage.removeItem('access_token');

        // Set auth status to false
        this.isAuthenticatedSubject.next(false);
    }

}
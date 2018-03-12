import { Component,OnDestroy,NgZone,OnInit,Input,DoCheck } from '@angular/core';
import {Subscription}   from 'rxjs/Subscription';
import {Router, ActivatedRoute, Params} from '@angular/router';
import {Http} from '@angular/http';
import {Observable} from 'rxjs/Observable';
import { DivsFlow,DivsFlowService,PolicyGroup,Policy,PolicyGroupWrap }  from '../pub.class';

@Component({
  moduleId: module.id,
  //selector: 'flow-main',
  //selector: 'my-app',
  templateUrl: '../../../templates/main.template.html'
})
export class FlowComponent implements OnDestroy,OnInit,DoCheck{
  private id: string;
  subscription:Subscription;
  flow:DivsFlow;
  policyGroups:Promise<PolicyGroup[]>;
  pgWrap:Promise<PolicyGroupWrap[]>;
  selectPolicyGroup:PolicyGroup=new PolicyGroup();
  //policyGroupPromise:Promise<PolicyGroup>;
  constructor(private router:Router,private route:ActivatedRoute,private http: Http, public flowService:DivsFlowService){
    route.params.subscribe(_ => this.id = _.id);
  }
  ngOnInit() {
    var eid=this.route.snapshot.params.id
    if(eid==undefined){
      console.log("fid is null");
      return;
    }
    this.route.params.subscribe(
        data => {          
          eid = data['id'];
          if(eid==undefined){
            console.log("fid is null");
            return;
          }
          console.log('queryParams', data['id']);
          this.getFlow(eid).then(flows => {
            if(flows.length==0){
              this.flow.fid=0;
              this.flow.title=null;
              this.flow.status=null;
            }
            return flows.find(f => {
                this.flow=f;
                this.policyGroups = this.flowService.getPolicyGroups(f.fid);
                return true;
            })
          });
        });
        console.log(this.policyGroups);
  }

  private getFlow(eid:number){
    //if ( ! this.flowsPromise)
    return this.http.get('/admin/action/flow/get?eid='+eid).toPromise().then(res => {
        var tmpflows:DivsFlow[];
        tmpflows = res.json() as DivsFlow[];
        for(var i=0;i<tmpflows.length;i++){
          if(tmpflows[i].fid == null)tmpflows[i].fid=0;
        }
        return tmpflows;
      });
  }
  ngOnDestroy(){
    // prevent memory leak when component destroyed
    //this.subscription.unsubscribe();
    console.log("ngOnDestroy");
  }

  ngDoCheck(){
  
    //console.log(this.route.queryParams.take.name);
  }

  private refushFlow(){
    this.getFlow(this.flow.eid).then(flows => flows.find(f => {
            this.flow=f;
            return true
    }));
  }
  onSyncFlow(fid:number) {
    if(window.confirm("确认要操作吗？")){
        this.http.put('/admin/action/flow/sync?fid='+fid,null).toPromise().then((response) => {
            if(response.status==200){
                this.refushFlow();
            }            
        });
    }
  }
  onSaveFlow() {     
     //this.router.navigate(['/flow',this.flow.fid,"1"], { queryParams:{fid:this.flow.fid},relativeTo:this.route});
     if(this.flow.title==null || this.flow.status==null){
       console.log(this.flow);
       alert("The Flow can't null");
       return false;
     }
     if(window.confirm("确认要操作吗？")){
       this.flow.status=Number(this.flow.status)
       //let options = new RequestOptions({ headers: headers });
        this.http.post('/admin/action/flow/set',JSON.stringify(this.flow)).toPromise().then((response) => {
            //do something...
            if(response.status==200){
                this.refushFlow();
            }              
        });
     }
  }

  addPolicyGroup(){
    //this.selectPolicyGroup=new PolicyGroup();
    this.router.navigate(["pg",0,this.flow.fid], { relativeTo:this.route});
  }

  onEditPG(policyGroup:PolicyGroup) {
   //this.policyGroupPromise = Promise.resolve(policyGroup);
   this.selectPolicyGroup=policyGroup;
   this.router.navigate(["pg",policyGroup.gid,this.flow.fid], { relativeTo:this.route});
   //this.router.navigateByUrl("/flow/"+this.flow.fid+"/"+policyGroup.gid);
   console.log(policyGroup);
   //this.router.navigate([{outlets: {page2: ['/flow',this.flow.fid,policyGroup.gid]}}]).then(_ => { });
   return false;
  }

  onDeletePG(pg:PolicyGroup) {
    if(window.confirm("确认要操作吗？")){
      this.http.delete('/admin/action/group/pg?gid='+pg.gid).toPromise().then((response) => {
          //do something...
          if(response.status==200){
            //alert('The PolicyGroup delete successful!');
            this.policyGroups = this.flowService.getPolicyGroups(this.flow.fid);
          }
          
      });
    }
    return false;
  }

  onAddPolicy(policyGroup:PolicyGroup) {
    console.log(policyGroup);
   this.selectPolicyGroup=policyGroup;
   this.router.navigate(["p",policyGroup.gid,0,this.flowService.getNextSeq()], { relativeTo:this.route});
   return false;
  }
  onEditPolicy(gid:number,policy:Policy) {
    console.log(policy);
   this.router.navigate(["p",gid,policy.id,this.flowService.getNextSeq()], { relativeTo:this.route});
   return false;
  }

  onDeletePolicy(p:Policy){
    if(window.confirm("确认要操作吗？")){
      this.http.delete('/admin/action/policy/del?pid='+p.id).toPromise().then((response) => {
          //do something...
          if(response.status==200){
            //alert('The Policy delete successful!');
            //this.getPolicys(p.id);
            this.policyGroups = this.flowService.getPolicyGroups(this.flow.fid);
          }
          
      });
    }
    return false;
  }

  openPopup(policyGroup:PolicyGroup) {
    this.router.navigate([{outlets: {popup: ['message', policyGroup.gid]}}]).then(_ => {
      // navigation is done
    });
  }
}

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
  templateUrl: '../../../templates/hisflows.template.html'
})
export class HisFlowComponent implements OnInit{
  private id: string;
  flows:DivsFlow[];
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
          this.getFlow(eid).then(flows => {
            this.flows=flows;
          });
        });
  }

  private getFlow(eid:number){
    return this.http.get('/admin/action/flow/get_his?eid='+eid).toPromise().then(res => {
      var tmp:DivsFlow[];
      tmp=res.json() as DivsFlow[];
      if (tmp instanceof Array)return tmp;
      return null;
    });
  }

}

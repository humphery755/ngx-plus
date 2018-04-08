import { Component,OnDestroy,NgZone,OnInit,Input } from '@angular/core';
import {Http} from '@angular/http';
import {Subscription}   from 'rxjs/Subscription';
import {Router, ActivatedRoute, Params} from '@angular/router';
import { DivsFlow,DivsFlowService,PolicyGroup,Policy }  from '../pub.class';

@Component({
    moduleId: module.id,
    selector: 'pg-detail',
    templateUrl: '../../../templates/pg-detail.template.html'
})
export class PGDetailComponent implements OnInit {
    editName:string;
    selectPolicyGroup:PolicyGroup=new PolicyGroup();
    constructor(private flowService:DivsFlowService,
                private route:ActivatedRoute,
                private router:Router,private http: Http) {

    }
    
    ngOnInit() {
        if(this.route.snapshot.params.id==undefined){
            console.log("gid is null");
            return;
        }

        

        this.route.params
            .switchMap((params:Params) =>this.flowService.getPolicyGroup(+params['id']))
            .subscribe((selectPolicyGroup:PolicyGroup) => {
                //this.editName = crisis.name;
                //console.log(this.route.snapshot.params.id);
                if(this.route.snapshot.params.id==0){
                    this.selectPolicyGroup=new PolicyGroup();
                    this.selectPolicyGroup.action=1;
                    this.selectPolicyGroup.uri="/";
                    this.selectPolicyGroup.level=1;
                    this.selectPolicyGroup.fid=this.route.snapshot.params.fid;
                    this.selectPolicyGroup.gid=0;
                }else
                this.selectPolicyGroup = selectPolicyGroup;
            });
    }

    onSavePolicyGroup(policyGroup:PolicyGroup) {
        this.http.post('/admin/action/group/set',JSON.stringify(this.selectPolicyGroup)).toPromise().then((response) => {
            if(response.status==200){
                var eid:number=this.route.parent.snapshot.params.id;
                this.router.navigate(['/admin/flow', eid,this.flowService.getNextSeq()], { relativeTo:this.route.parent.parent});
            }            
        });
        
    }
    
}

@Component({
    moduleId: module.id,
    selector: 'crisis-center',
    template: `<h2>危机中心</h2>
<router-outlet></router-outlet>`
})
export class NothingComponent implements OnInit {
    constructor() {
    }
    ngOnInit() {
    }

}
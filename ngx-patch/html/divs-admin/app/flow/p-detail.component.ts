import { Component,OnDestroy,NgZone,OnInit,Input ,ElementRef} from '@angular/core';
import {Http} from '@angular/http';
import {Subscription}   from 'rxjs/Subscription';
import {Router, ActivatedRoute, Params} from '@angular/router';
import { DivsFlow,DivsFlowService,PolicyGroup,Policy,PolicyData }  from '../pub.class';

@Component({
    moduleId: module.id,
    templateUrl: '../../../templates/p-detail.template.html'
})
export class PDetailComponent implements OnInit {
    pageLen:number;
    pageIndex:number;
    selectPolicy:Policy=new Policy();
    policyDatas:Promise<PolicyData[]>;

    constructor(private flowService:DivsFlowService,
                private route:ActivatedRoute,
                private router:Router,private http: Http) {
    }
    
    ngOnInit() {
        if(this.route.snapshot.params.pid==undefined){
            console.log("pid is null");
            return;
        }
        this.pageIndex=0;
        this.route.params
            .switchMap((params:Params) =>this.flowService.getPolicy(+params['pid']))
            .subscribe((selectPolicy:Policy) => {
                if(this.route.snapshot.params.pid==0){
                    this.selectPolicy.id=0;
                    this.selectPolicy.type=null;
                    this.selectPolicy.gid=this.route.snapshot.params.gid;
                }else{
                    this.selectPolicy = selectPolicy;
                }                
                this.refreshPage(this.selectPolicy);
                $('#exampleModal').modal('show');
                $("#exampleAlert").alert("close");
            });
    }
    onFirstPage(policy:Policy) {
        this.pageIndex=0;
        this.refreshPage(policy);
        return false;
    }
    onPreviousPage(policy:Policy) {
        this.pageIndex--;
        this.refreshPage(policy);
        return false;
    }
    onNextPage(policy:Policy) {
        this.pageIndex++;
        this.refreshPage(policy);
        return false;
    }
    private refreshPage(policy:Policy){
        this.policyDatas = this.flowService.getPolicyDatas(this.pageIndex,policy.id).then(res => {
            if(res instanceof Array){
                this.pageLen=res.length;
                if (this.pageLen>15)
                res.pop();
            }
            return res;
        });
    }
    onSavePolicy() {
        if(!this.selectPolicy.type){
            alert("'type' is null");
            return false;
        }
        if(window.confirm("确认要操作吗？")){
            this.http.post('/admin/action/policy/set',JSON.stringify(this.selectPolicy)).toPromise().then((response) => {
                //do something...
                if(response.status==200){
                    //alert('The Policy save successful!');
                    //$(jQuery).bootstrapGrowl("The Policy save successful!");
                    
                    //setTimeout(function(){$("#exampleAlert").alert("close");},2000);
                    var insert_id:number;
                    insert_id=JSON.parse(response.text()).insert_id;
                    if(insert_id!=null){
                        this.selectPolicy.id=insert_id;
                    }
                    this.refreshPage(this.selectPolicy);
                }                
            });
        }
    }

    onEditPD(pd:PolicyData,ptype:string) {
        this.router.navigate(["pd",ptype,pd.pid,pd.id,this.flowService.getNextSeq()], { relativeTo:this.route});
        return false;
    }

    onDeletePD(pd:PolicyData) {
        if(window.confirm("确认要删除吗？")){
            this.http.delete('/admin/action/policy/data/del?id='+pd.id+"&ptype="+this.selectPolicy.type).toPromise().then((response) => {
                if(response.status==200){
                    this.refreshPage(this.selectPolicy);
                }            
            });
        }
        return false;
    }
    onSavePD() {
        if(!window.confirm("确认要操作吗？"))return false;
        var pd:PolicyData=this.flowService.getLastPolicyData();
        var datas=new Array();
        datas.push(pd);
        this.http.post('/admin/action/policy/data/set?ptype='+this.selectPolicy.type,JSON.stringify(datas)).toPromise().then((response) => {
            //do something...
            if(response.status==200){
                //alert('The Policy Data saved successful!');
                this.refreshPage(this.selectPolicy);
            }            
        });
    }
    onAddPD() {
        this.router.navigate(["pd",this.selectPolicy.type,this.selectPolicy.id,0,this.flowService.getNextSeq()], { relativeTo:this.route});
    }
    onAddPDCID() {
        this.router.navigate(["pd",this.selectPolicy.type,this.selectPolicy.id,0,this.flowService.getNextSeq()], { relativeTo:this.route});
    }
    onAddPDAID() {
        this.router.navigate(["pd",this.selectPolicy.type,this.selectPolicy.id,0,this.flowService.getNextSeq()], { relativeTo:this.route});
    }
}

@Component({
    moduleId: module.id,
    selector: 'crisis-center',
    template: `
    <table class="table" *ngIf="policyData">        
        <tr *ngIf="ptype=='aidrange' || ptype=='cidrange' || ptype=='iprange'">
            <td>pdid:</td>
            <td>{{policyData.id}}</td>
            <td>start:</td>
            <td><input type="number" name="start" class="form-control" [(ngModel)]="policyData.start"></td>
            <td>end:</td>
            <td><input type="number" name="start" class="form-control" [(ngModel)]="policyData.end"></td>
        </tr>
        <tr *ngIf="ptype=='aidset' || ptype=='aidsuffix' || ptype=='cidset' || ptype=='cidsuffix' || ptype=='oidset' || ptype=='ipset'">
            <td>pdid:</td>
            <td>{{policyData.id}}</td>
            <td>val:</td>
            <td><input type="number" name="val" class="form-control" [(ngModel)]="policyData.val"></td>
        </tr>
    </table>
`
})
export class PDDetailComponent implements OnInit {
    ptype:string;
    policyData:PolicyData;
    constructor(private flowService:DivsFlowService,
                private route:ActivatedRoute,
                private router:Router) {
    }
    ngOnInit() {
        if(this.route.snapshot.params.id==undefined){
            console.log("pdid is null");
            return;
        }

        this.route.params
            .switchMap((params:Params) =>this.flowService.getPolicyData(+params['id']))
            .subscribe((policyData:PolicyData) => {
                this.ptype = this.route.snapshot.params.ptype;
                this.policyData = policyData;
                if(this.route.snapshot.params.id==0){
                    policyData.pid=this.route.snapshot.params.pid;
                }
            });
    }

}

@Component({
    moduleId: module.id,
    selector: 'crisis-center',
    template: `
    no implement
`
})
export class CidsetComponent implements OnInit {
    ptype:string;
    policyData:PolicyData;
    constructor(private flowService:DivsFlowService,
                private route:ActivatedRoute,
                private router:Router) {
    }
    ngOnInit() {
        if(this.route.snapshot.params.id==undefined){
            console.log("pdid is null");
            return;
        }

        this.route.params
            .switchMap((params:Params) =>this.flowService.getPolicyData(+params['id']))
            .subscribe((policyData:PolicyData) => {
                this.ptype = this.route.snapshot.params.ptype;
                this.policyData = policyData;
                if(this.route.snapshot.params.id==0){
                    policyData.pid=this.route.snapshot.params.pid;
                }
            });
    }

}

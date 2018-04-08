import {Component, OnInit} from '@angular/core';
import {Meta} from '@angular/platform-browser';
import {Router, ActivatedRoute, Params} from '@angular/router';
import {Observable} from 'rxjs/Observable';
import 'rxjs/add/operator/switchMap';
import { DivsEnv,DivsFlow,DivsFlowService ,User,UserService}  from './pub.class';

//export const ROUTER_DIRECTIVES = [RouterOutlet, RouterLink, RouterLinkWithHref, RouterLinkActive];

@Component({
  moduleId:module.id,
  selector: 'app-root',
  templateUrl:'app.component.html',
    styleUrls:[]
})

export class AppComponent  implements OnInit {
  title = '英雄帖';

    isSingle:boolean;

    constructor(private router:Router,private route:ActivatedRoute,
        private userService:UserService
    ){}

    ngOnInit(): void {
        this.userService.populate();
        this.userService.isAuthenticated.subscribe(
            (isAuthenticated)=>{
                console.log(isAuthenticated);
                this.isSingle = !isAuthenticated;
                if(isAuthenticated)this.router.navigateByUrl('/admin');
                else
                this.router.navigateByUrl('/login');
            }
        )
    }
}

@Component({
  moduleId:module.id,
  selector: 'app-root',
  //directives: [ROUTER_DIRECTIVES],
   templateUrl: './../templates/divs.admin.html'
})

export class DivsAdminComponent  implements OnInit {
    public selectedEnv: DivsEnv;
    envTrees:Observable<DivsEnv[]>;
    constructor(private router:Router,private route:ActivatedRoute,private flowService: DivsFlowService) {
     // this.mainPage = new MainPageComponent();
    }

    ngOnInit() {
      //this.flows = this.flowService.getFlows();
      this.envTrees = this.route.params
            .switchMap((params:Params) => {
                return this.flowService.getEnvTree();
            })
    }

    onSelect(env: DivsEnv) {
        this.selectedEnv = env;
        if(env!=null){
          //console.log(env);
          this.router.navigate(['flow', env.eid,this.flowService.getNextSeq()],{ relativeTo:this.route });
          //this.router.navigate([{outlets: {root_routlet: ['flow', flow.fid]}}]).then(_ => {});
          //this.router.navigate(['/crisis-center', {fid:flow.fid}]); this.router.navigate(['/flow', {fid:flow.fid}],{ queryParams: { fid:flow.fid} });
          
        }
        return false;
        //console.log(this.fcService.change);
        //console.log(flow);
  }

   onListHisFlows(env: DivsEnv) {
        if(env!=null){
          this.router.navigate(['his/flow', env.eid,this.flowService.getNextSeq()],{ relativeTo:this.route });
        }
        return false;
  }

  onListNodeInfo() {
        console.log("monitor/nodes");
        this.router.navigate(['monitor/nodes', this.flowService.getNextSeq()],{ relativeTo:this.route });
        //this.router.navigateByUrl('/monitor/nodes/'+this.flowService.getNextSeq());
        
        return false;
  }
}

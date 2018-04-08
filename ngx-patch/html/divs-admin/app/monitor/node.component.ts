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
  templateUrl: '../../../templates/nodes.template.html'
})
export class NodeComponent implements OnInit{
  private id: string;
  nodes:any[];
  constructor(private router:Router,private route:ActivatedRoute,private http: Http){
    route.params.subscribe(_ => this.id = _.id);
  }
  ngOnInit() {
    this.route.params.subscribe(
        data => {
          this.getNodes().then(nodes => {
            this.nodes=nodes;
          });
        });
  }

  private getNodes(){
    return this.http.get('/admin/action/monitor/get_node_info').toPromise().then(res => res.json() as any[]);
  }
  
}

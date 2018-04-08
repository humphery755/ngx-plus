import {NgModule} from '@angular/core';
import {FormsModule} from '@angular/forms';
import {CommonModule} from '@angular/common';
import {MonitorRoutingModule} from "./monitor-routing.module";
import {NodeComponent} from "./node.component";
import { DivsFlowChangeService }  from '../pub.class';

@NgModule({
    imports:[
        FormsModule,
        CommonModule,
        MonitorRoutingModule
    ],
    declarations:[
        NodeComponent
    ],
    providers:[
        DivsFlowChangeService
    ]
})

export class MonitorModule{

}
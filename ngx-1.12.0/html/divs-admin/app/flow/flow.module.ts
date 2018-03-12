import {NgModule} from '@angular/core';
import {FormsModule} from '@angular/forms';
import {CommonModule} from '@angular/common';
import {FlowRoutingModule} from "./flow-routing.module";
import {FlowComponent} from "./flow.component";
//import {PGListComponent} from "./pg-list.component";
import {PGDetailComponent,NothingComponent} from "./pg-detail.component";
import {PDetailComponent,PDDetailComponent} from "./p-detail.component";
import {HisFlowComponent} from "./his.component";
import { DivsFlowChangeService }  from '../pub.class';
import { DivsAdminComponent }  from '../app.component';

@NgModule({
    imports:[
        FormsModule,
        CommonModule,
        FlowRoutingModule
    ],
    declarations:[
        FlowComponent,
        PGDetailComponent,
        PDetailComponent,
        PDDetailComponent,
        NothingComponent,
        HisFlowComponent,
        DivsAdminComponent
    ],
    providers:[
        DivsFlowChangeService
    ]

})

export class FlowModule{

}
import {NgModule} from '@angular/core';
import {RouterModule, Routes} from '@angular/router';

import {NodeComponent} from "./node.component";

import {NotFoundComponent} from '../not-found.component'

const monitorRoutes:Routes = [
    {
        path:"",
        component:NodeComponent
    },
    {
        path:"monitor/nodes/:seq",
        component:NodeComponent
    }
];

@NgModule({
    imports:[
        //ModalModule.forRoot(),
        RouterModule.forChild(monitorRoutes)
    ],
    exports:[
        RouterModule
    ]
})

export class MonitorRoutingModule{

}

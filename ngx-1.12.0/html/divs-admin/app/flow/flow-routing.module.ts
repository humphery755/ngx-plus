import {NgModule} from '@angular/core';
import {RouterModule, Routes} from '@angular/router';

//import { ModalSectionComponent } from './modal-section.component';
import {FlowComponent} from "./flow.component";
import {HisFlowComponent} from "./his.component";
import {PGDetailComponent,NothingComponent} from "./pg-detail.component";
import {PDetailComponent,PDDetailComponent} from "./p-detail.component";
import {LoginComponent} from "../login/login.component";
import { DivsAdminComponent }  from '../app.component';
import {NotFoundComponent} from '../not-found.component'
import {NodeComponent} from "../monitor/node.component";

const flowRoutes:Routes = [    
    {
        path:"login",
        component:LoginComponent
    },
    {
        path:"admin",
        component:DivsAdminComponent,
        children:[
            {
                path:"flow/:id/:nextseq",
                component:FlowComponent,
                children:[
                    {
                        path:"pg/:id/:fid",
                        component:PGDetailComponent
                    },
                    {
                        path:"p/:gid/:pid/:nextseq",
                        component:PDetailComponent,
                        children:[
                            {
                                        path:"pd/:ptype/:pid/:id/:nextseq",
                                        component:PDDetailComponent
                            }
                        ]
                    }            
                ]
            },
            {
                path:"his/flow/:id/:nextseq",
                component:HisFlowComponent
            },
            {
                path:"monitor/nodes/:seq",
                component:NodeComponent
            }
        ]
    }
];

@NgModule({
    imports:[
        //ModalModule.forRoot(),
        RouterModule.forChild(flowRoutes)
    ],
    exports:[
        RouterModule
    ]
})

export class FlowRoutingModule{

}

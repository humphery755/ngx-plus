"use strict";
var __decorate = (this && this.__decorate) || function (decorators, target, key, desc) {
    var c = arguments.length, r = c < 3 ? target : desc === null ? desc = Object.getOwnPropertyDescriptor(target, key) : desc, d;
    if (typeof Reflect === "object" && typeof Reflect.decorate === "function") r = Reflect.decorate(decorators, target, key, desc);
    else for (var i = decorators.length - 1; i >= 0; i--) if (d = decorators[i]) r = (c < 3 ? d(r) : c > 3 ? d(target, key, r) : d(target, key)) || r;
    return c > 3 && r && Object.defineProperty(target, key, r), r;
};
Object.defineProperty(exports, "__esModule", { value: true });
var core_1 = require("@angular/core");
var router_1 = require("@angular/router");
//import { ModalSectionComponent } from './modal-section.component';
var flow_component_1 = require("./flow.component");
var his_component_1 = require("./his.component");
var pg_detail_component_1 = require("./pg-detail.component");
var p_detail_component_1 = require("./p-detail.component");
var login_component_1 = require("../login/login.component");
var app_component_1 = require("../app.component");
var node_component_1 = require("../monitor/node.component");
var flowRoutes = [
    {
        path: "login",
        component: login_component_1.LoginComponent
    },
    {
        path: "admin",
        component: app_component_1.DivsAdminComponent,
        children: [
            {
                path: "flow/:id/:nextseq",
                component: flow_component_1.FlowComponent,
                children: [
                    {
                        path: "pg/:id/:fid",
                        component: pg_detail_component_1.PGDetailComponent
                    },
                    {
                        path: "p/:gid/:pid/:nextseq",
                        component: p_detail_component_1.PDetailComponent,
                        children: [
                            {
                                path: "pd/:ptype/:pid/:id/:nextseq",
                                component: p_detail_component_1.PDDetailComponent
                            }
                        ]
                    }
                ]
            },
            {
                path: "his/flow/:id/:nextseq",
                component: his_component_1.HisFlowComponent
            },
            {
                path: "monitor/nodes/:seq",
                component: node_component_1.NodeComponent
            }
        ]
    }
];
var FlowRoutingModule = (function () {
    function FlowRoutingModule() {
    }
    return FlowRoutingModule;
}());
FlowRoutingModule = __decorate([
    core_1.NgModule({
        imports: [
            //ModalModule.forRoot(),
            router_1.RouterModule.forChild(flowRoutes)
        ],
        exports: [
            router_1.RouterModule
        ]
    })
], FlowRoutingModule);
exports.FlowRoutingModule = FlowRoutingModule;
//# sourceMappingURL=flow-routing.module.js.map
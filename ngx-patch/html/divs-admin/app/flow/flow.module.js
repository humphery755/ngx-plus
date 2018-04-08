"use strict";
var __decorate = (this && this.__decorate) || function (decorators, target, key, desc) {
    var c = arguments.length, r = c < 3 ? target : desc === null ? desc = Object.getOwnPropertyDescriptor(target, key) : desc, d;
    if (typeof Reflect === "object" && typeof Reflect.decorate === "function") r = Reflect.decorate(decorators, target, key, desc);
    else for (var i = decorators.length - 1; i >= 0; i--) if (d = decorators[i]) r = (c < 3 ? d(r) : c > 3 ? d(target, key, r) : d(target, key)) || r;
    return c > 3 && r && Object.defineProperty(target, key, r), r;
};
Object.defineProperty(exports, "__esModule", { value: true });
var core_1 = require("@angular/core");
var forms_1 = require("@angular/forms");
var common_1 = require("@angular/common");
var flow_routing_module_1 = require("./flow-routing.module");
var flow_component_1 = require("./flow.component");
//import {PGListComponent} from "./pg-list.component";
var pg_detail_component_1 = require("./pg-detail.component");
var p_detail_component_1 = require("./p-detail.component");
var his_component_1 = require("./his.component");
var pub_class_1 = require("../pub.class");
var app_component_1 = require("../app.component");
var FlowModule = (function () {
    function FlowModule() {
    }
    return FlowModule;
}());
FlowModule = __decorate([
    core_1.NgModule({
        imports: [
            forms_1.FormsModule,
            common_1.CommonModule,
            flow_routing_module_1.FlowRoutingModule
        ],
        declarations: [
            flow_component_1.FlowComponent,
            pg_detail_component_1.PGDetailComponent,
            p_detail_component_1.PDetailComponent,
            p_detail_component_1.PDDetailComponent,
            pg_detail_component_1.NothingComponent,
            his_component_1.HisFlowComponent,
            app_component_1.DivsAdminComponent
        ],
        providers: [
            pub_class_1.DivsFlowChangeService
        ]
    })
], FlowModule);
exports.FlowModule = FlowModule;
//# sourceMappingURL=flow.module.js.map
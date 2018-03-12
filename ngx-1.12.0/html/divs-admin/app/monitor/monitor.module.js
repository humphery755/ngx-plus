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
var monitor_routing_module_1 = require("./monitor-routing.module");
var node_component_1 = require("./node.component");
var pub_class_1 = require("../pub.class");
var MonitorModule = (function () {
    function MonitorModule() {
    }
    return MonitorModule;
}());
MonitorModule = __decorate([
    core_1.NgModule({
        imports: [
            forms_1.FormsModule,
            common_1.CommonModule,
            monitor_routing_module_1.MonitorRoutingModule
        ],
        declarations: [
            node_component_1.NodeComponent
        ],
        providers: [
            pub_class_1.DivsFlowChangeService
        ]
    })
], MonitorModule);
exports.MonitorModule = MonitorModule;
//# sourceMappingURL=monitor.module.js.map
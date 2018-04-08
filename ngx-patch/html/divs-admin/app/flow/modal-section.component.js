"use strict";
var __decorate = (this && this.__decorate) || function (decorators, target, key, desc) {
    var c = arguments.length, r = c < 3 ? target : desc === null ? desc = Object.getOwnPropertyDescriptor(target, key) : desc, d;
    if (typeof Reflect === "object" && typeof Reflect.decorate === "function") r = Reflect.decorate(decorators, target, key, desc);
    else for (var i = decorators.length - 1; i >= 0; i--) if (d = decorators[i]) r = (c < 3 ? d(r) : c > 3 ? d(target, key, r) : d(target, key)) || r;
    return c > 3 && r && Object.defineProperty(target, key, r), r;
};
Object.defineProperty(exports, "__esModule", { value: true });
var core_1 = require("@angular/core");
//import { DEMOS } from './demos';
// webpack html imports
var titleDoc = require('html-loader!markdown-loader!./docs/title.md');
var ModalSectionComponent = (function () {
    function ModalSectionComponent() {
        this.name = 'Modals';
        this.src = 'https://github.com/valor-software/ngx-bootstrap/tree/master/components/modal';
        this.titleDoc = titleDoc;
    }
    return ModalSectionComponent;
}());
ModalSectionComponent = __decorate([
    core_1.Component({
        selector: 'modal-section',
        template: "\n<demo-section [name]=\"name\" [src]=\"src\">\n  <p>Modals are streamlined, but flexible, dialog prompts with the minimum required functionality and smart defaults.</p>\n\n  <h2>Contents</h2>\n  <ul>\n    <li><a routerLink=\".\" fragment=\"usage\">Usage</a></li>\n    <li><a routerLink=\".\" fragment=\"examples\">Examples</a>\n      <ul>\n        <li><a routerLink=\".\" fragment=\"static\">Static modal</a></li>\n        <li><a routerLink=\".\" fragment=\"sizes\">Optional sizes</a></li>\n        <li><a routerLink=\".\" fragment=\"child\">Child modal</a></li>\n        <li><a routerLink=\".\" fragment=\"auto-shown\">Auto shown modal</a></li>\n      </ul>\n    </li>\n    <li><a routerLink=\".\" fragment=\"api-reference\">API Reference</a>\n      <ul>\n        <li><a routerLink=\".\" fragment=\"modal-directive\">ModalDirective</a></li>\n        <li><a routerLink=\".\" fragment=\"modal-backdrop-component\">ModalBackdropComponent</a></li>\n        <li><a routerLink=\".\" fragment=\"modal-options\">ModalOptions</a></li>\n      </ul>\n    </li>\n  </ul>\n\n  <h2 routerLink=\".\" fragment=\"usage\" id=\"usage\">Usage</h2>\n\n  <p [innerHtml]=\"titleDoc\"></p>\n\n  <h2 routerLink=\".\" fragment=\"examples\" id=\"examples\">Examples</h2>\n\n  <h2 routerLink=\".\" fragment=\"static\" id=\"static\">Static modal</h2>\n\n\n  <h2 routerLink=\".\" fragment=\"sizes\" id=\"sizes\">Optional sizes</h2>\n\n\n  <h2 routerLink=\".\" fragment=\"child\" id=\"child\">Child modal</h2>\n  <p>Control modal from parent component</p>\n\n\n  <h2 routerLink=\".\" fragment=\"auto-shown\" id=\"auto-shown\">Auto shown modal</h2>\n  <p>\n    Show modal right after it has been initialized.\n    This allows you to keep DOM clean by only appending visible modals to the DOM using <code>*ngIf</code> directive.\n  </p>\n  <p>\n    It can also be useful if you want your modal component to perform some initialization operations, but\n    want to defer that until user actually sees modal content. I.e. for a \"Select e-mail recipient\" modal\n    you might want to defer recipient list loading until the modal is shown.\n  </p>\n\n\n  <h2 routerLink=\".\" fragment=\"api-reference\" id=\"api-reference\">API Reference</h2>\n  <ng-api-doc routerLink=\".\" fragment=\"modal-directive\" id=\"modal-directive\" directive=\"ModalDirective\"></ng-api-doc>\n  <ng-api-doc routerLink=\".\" fragment=\"modal-backdrop-component\" id=\"modal-backdrop-component\" directive=\"ModalBackdropComponent\"></ng-api-doc>\n  <ng-api-doc-config routerLink=\".\" fragment=\"modal-options\" id=\"modal-options\" type=\"ModalOptions\"></ng-api-doc-config>\n</demo-section>"
    })
], ModalSectionComponent);
exports.ModalSectionComponent = ModalSectionComponent;
//# sourceMappingURL=modal-section.component.js.map
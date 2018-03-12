import { NgModule }      from '@angular/core';
//import { ROUTER_DIRECTIVES } from '@angular/router-deprecated';
import { BrowserModule } from '@angular/platform-browser';
import { FormsModule } from '@angular/forms';
import { HttpModule } from '@angular/http';
import { InMemoryWebApiModule } from 'angular-in-memory-web-api';

import { AppComponent }  from './app.component';
import {AppRoutingModule} from './app-routing.module';
import {FlowModule} from './flow/flow.module';
import {NotFoundComponent} from './not-found.component';
import { DivsFlowService,DivsFlowChangeService,UserService }  from './pub.class';
import {DialogService} from './dialog.service';

import {MonitorModule} from './monitor/monitor.module';
import {LoginModule} from './login/login.module';
import {LoginComponent} from "./login/login.component";

@NgModule({
  imports:      [ 
    BrowserModule,
    FormsModule,
    HttpModule,
    FlowModule,
    MonitorModule,
    LoginModule,
    AppRoutingModule
  ],
  declarations: [ 
    AppComponent,
    NotFoundComponent
  ],
  providers:[
    DivsFlowService,
    DivsFlowChangeService,
    UserService
  ],
  bootstrap:    [ AppComponent]
})
export class AppModule { }

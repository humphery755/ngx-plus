// /**
//  * Created by yitala on 2016/12/6.
//  */
 import {NgModule} from "@angular/core";
 import {RouterModule, Routes} from '@angular/router';
import {FormsModule} from '@angular/forms';
import {CommonModule} from '@angular/common';
 import {LoginComponent} from "./login.component";
 import {NeedAuthGuard} from "../login/no-auth-guard.service";
 @NgModule({
     imports:[
        FormsModule,
        CommonModule,
        RouterModule.forChild([
             {
                 path:'',component:LoginComponent,canActivate:[NeedAuthGuard]
             }
         ])
     ],
     declarations:[
         LoginComponent
     ],
     providers:[
         NeedAuthGuard
     ]
 })

 export class LoginModule{

 }
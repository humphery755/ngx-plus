/**
 * Created by yitala on 2016/12/6.
 */
import {Component} from "@angular/core";
import {FormBuilder, Form, FormGroup} from "@angular/forms";
import {ActivatedRoute, Router} from "@angular/router";
import {User,UserService} from '../pub.class';

import {Http,RequestOptions} from '@angular/http';

@Component({
    moduleId:module.id,
    selector: 'app-root',
    templateUrl:'../../templates/login.component.html',
    styleUrls:['../../templates/login.component.css']
})

export class LoginComponent{

    model:any = {username:'',password:''};
    error:string;

    constructor(private http: Http,
        private router:Router,
        private userService:UserService
    ){

    }

    login(){
        let user = this.model as User;
        var body='username='+user.username+'&passwd='+user.password;
        this.http.put('/admin/action/login/user',body).toPromise().then(res => {
            var tmp:any = res.json();
            if(tmp.code==0){
                user.token = tmp.token
                this.userService.setAuth(user);
                this.router.navigateByUrl('/admin');
            }
            else {
                this.error = '用户名或密码错误'
            }
            return null;
        });
    }
}
/**
 * Created by yitala on 2016/12/6.
 */
import {Injectable} from "@angular/core";
import {CanActivate, ActivatedRouteSnapshot, RouterStateSnapshot, Router} from "@angular/router";
import {Observable} from 'rxjs/Observable';
import { UserService}  from '../pub.class';
@Injectable()
export class NeedAuthGuard implements CanActivate{

    private result:boolean;

    constructor(
        private router:Router,
        private userService:UserService
    ){}

    canActivate(route: ActivatedRouteSnapshot, state: RouterStateSnapshot): Observable<boolean>|Promise<boolean>|boolean {
        console.log(this.userService.isAuthenticated);
        this.userService.isAuthenticated.subscribe(
            (isAuthenticated) => {
                if (!isAuthenticated) {
                    this.router.navigate(["/login"]);
                }
                this.result = isAuthenticated;
            }
        )
        return this.result;
    }

}
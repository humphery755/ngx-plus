/*
SQLyog v10.2 
MySQL - 5.6.35-log : Database - diversion
*********************************************************************
*/
/*!40101 SET NAMES utf8 */;

/*!40101 SET SQL_MODE=''*/;

/*!40014 SET @OLD_UNIQUE_CHECKS=@@UNIQUE_CHECKS, UNIQUE_CHECKS=0 */;
/*!40014 SET @OLD_FOREIGN_KEY_CHECKS=@@FOREIGN_KEY_CHECKS, FOREIGN_KEY_CHECKS=0 */;
/*!40101 SET @OLD_SQL_MODE=@@SQL_MODE, SQL_MODE='NO_AUTO_VALUE_ON_ZERO' */;
/*!40111 SET @OLD_SQL_NOTES=@@SQL_NOTES, SQL_NOTES=0 */;
CREATE DATABASE /*!32312 IF NOT EXISTS*/`diversion` /*!40100 DEFAULT CHARACTER SET utf8 */;

USE `diversion`;

/*Table structure for table `t_diversion` */

CREATE TABLE `t_diversion` (
  `id` int(11) NOT NULL,
  `title` varchar(100) NOT NULL,
  `crt_date` timestamp NOT NULL,
  `status` int(11) NOT NULL,
  `md5` varchar(50) DEFAULT NULL,
  `ver` int(11) DEFAULT '1',
  PRIMARY KEY (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;


CREATE TABLE `t_diversion_plugins` (
  `name` varchar(100) NOT NULL,
  `desc` varchar(200) NOT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

/*Table structure for table `t_policy_action` */

CREATE TABLE `t_policy_action` (
  `id` int(11) NOT NULL,
  `desc` varchar(100) NOT NULL,
  PRIMARY KEY (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

/*Data for the table `t_policy_action` */

insert  into `t_policy_action`(`id`,`desc`) values (0,'redirect');
insert  into `t_policy_action`(`id`,`desc`) values (1,'given upstream');
insert  into `t_policy_action`(`id`,`desc`) values (3,'plugins: content_filter');
insert  into `t_policy_action`(`id`,`desc`) values (4,'plugins: header_filter');
insert  into `t_policy_action`(`id`,`desc`) values (5,'plugins: body_filter');

/*Table structure for table `t_policy_group` */

CREATE TABLE `t_policy_group` (
  `id` int(11) NOT NULL,
  `div_id` int(11) NOT NULL,
  `uri` varchar(200) NOT NULL,
  `action` int(11) NOT NULL,
  `backend` varchar(500) NOT NULL,
  PRIMARY KEY (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;


/*Table structure for table `t_policy_numset` */

CREATE TABLE `t_policy` (
  `id` bigint(20) NOT NULL AUTO_INCREMENT,
  `gid` int(11) NOT NULL,
  `type` varchar(50) NOT NULL,
  PRIMARY KEY (`id`)
) ENGINE=InnoDB AUTO_INCREMENT=3 DEFAULT CHARSET=utf8;

/*Table structure for table `t_policy_numset` */

CREATE TABLE `t_policy_numset` (
  `id` bigint(20) NOT NULL AUTO_INCREMENT,
  `pid` int(11) NOT NULL,
  `val` bigint(20) NOT NULL,
  PRIMARY KEY (`id`)
) ENGINE=InnoDB AUTO_INCREMENT=3 DEFAULT CHARSET=utf8;


/*Table structure for table `t_policy_range` */

CREATE TABLE `t_policy_range` (
  `id` int(11) NOT NULL AUTO_INCREMENT,
  `pid` int(11) NOT NULL,
  `start` bigint(20) NOT NULL,
  `end` bigint(20) NOT NULL,
  PRIMARY KEY (`id`)
) ENGINE=InnoDB AUTO_INCREMENT=3 DEFAULT CHARSET=utf8;


/*Table structure for table `t_policy_strset` */

CREATE TABLE `t_policy_strset` (
  `id` bigint(20) NOT NULL AUTO_INCREMENT,
  `pid` int(11) NOT NULL,
  `val` varchar(100) NOT NULL,
  PRIMARY KEY (`id`)
) ENGINE=InnoDB AUTO_INCREMENT=4 DEFAULT CHARSET=utf8;


/*Table structure for table `t_policy_type` */

CREATE TABLE `t_policy_type` (
  `cd` varchar(50) NOT NULL,
  `desc` varchar(100) NOT NULL,
  UNIQUE KEY `IDX_POLICY_TYPE` (`cd`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

/*Data for the table `t_policy_type` */

insert  into `t_policy_type`(`cd`,`desc`) values ('aidrange','aid range');
insert  into `t_policy_type`(`cd`,`desc`) values ('aidset','aid set');
insert  into `t_policy_type`(`cd`,`desc`) values ('aidsuffix','aid suffix');
insert  into `t_policy_type`(`cd`,`desc`) values ('cidrange','cid range');
insert  into `t_policy_type`(`cd`,`desc`) values ('cidset','cid set');
insert  into `t_policy_type`(`cd`,`desc`) values ('cidsuffix','cid suffix');
insert  into `t_policy_type`(`cd`,`desc`) values ('iprange','ip range');

/*!40101 SET SQL_MODE=@OLD_SQL_MODE */;
/*!40014 SET FOREIGN_KEY_CHECKS=@OLD_FOREIGN_KEY_CHECKS */;
/*!40014 SET UNIQUE_CHECKS=@OLD_UNIQUE_CHECKS */;
/*!40111 SET SQL_NOTES=@OLD_SQL_NOTES */;
DELIMITER $$

ALTER DEFINER=`yuhui`@`%` EVENT `update_divs_ver` ON SCHEDULE EVERY 10 SECOND STARTS '2017-04-11 20:58:25' ON COMPLETION NOT PRESERVE ENABLE COMMENT 'update ver of diversion the table each second' DO BEGIN
	
	    UPDATE t_diversion SET ver=ver+1;
	    
	END$$

DELIMITER ;
/// riskserv.cpp 风险普查成果检查http服务版
/// wf 2022-1-14

/// V1.0.0 created 2022-1-14 wf


#include "RiskValidateTool.h"
#include <string>
#include <iostream>
#include "ajson5.h"
#include "spdlog/spdlog.h"
#include "spdlog/sinks/basic_file_sink.h"
#include "spdlog/sinks/daily_file_sink.h"
#include "wdirtools.h"
#include <fstream>
#include "wstringutils.h"
#include "httplib.h"


using namespace std;




int main(int argc , char * argv[]){
	string version = "v1.0.0 2022-1-14" ;
	version += "\nv1.1.0 2022-1-20" ;

	cout<<"A program to server risk product validate. by wf 2022-1-14."<<endl ;
	cout<<version<<endl ;
	cout<<"usage:riskserv port /some/dir/logfilename"<<endl ;

	cout<<"检验算法版本："<<RiskValidateTool::getVersion() <<endl ;
	cout<<"当前检验算法不对CGCS2000进行判断！！ 2022-1-20"<<endl ;
	GDALAllRegister();
	if( argc!=3 ){
		cout<<"argc not 3."<<endl ;
		return 11 ;
	}

	string port = argv[1];
	string logfile = argv[2] ;
    auto dailylogger = spdlog::daily_logger_mt("l", logfile.c_str() , 2, 0) ;//2:00 am
    spdlog::flush_every(std::chrono::seconds(1) ) ;
    spdlog::set_default_logger(dailylogger );
    spdlog::info("****************************");
    spdlog::info("riskserv version:{}",version);
    spdlog::info("检验算法版本：{} ， 当前检验算法不对CGCS2000进行判断！！ 2022-1-20 ", RiskValidateTool::getVersion() );
    spdlog::info("port:{}" , port);

    string exefilename = argv[0] ;
    string exedir = wDirTools::parentDir(exefilename) ;
    string standardCodeRasterFile = exedir + "/" + "risk_standard_grid_code_V200.tif";
    spdlog::info("try to load {}" , standardCodeRasterFile) ;
    bool standardExist = wDirTools::isFileExist(standardCodeRasterFile) ;
    if( standardExist==false ){
        spdlog::error ("打开标准网格数据文件失败 {}" ,standardCodeRasterFile ) ;
        return 12 ;
    }
    

    // HTTP
	httplib::Server svr;
    svr.Get("/validate", [standardCodeRasterFile](const httplib::Request &req, httplib::Response &res) {
    	string state = "1" ;// 0-success, 1-do nothing, 9-validate error.
    	string message =  "do nothing" ;
    	if( req.has_param("infile") ){
    		 string inputFilename = req.get_param_value("infile");
    		 spdlog::info("try to check {}" , inputFilename);
    		//0.检查数据类型与取值范围
		    string error01 ;
		    bool ok01 = RiskValidateTool::isGeoTiff_Integer(inputFilename,error01) ;
		    spdlog::info("输入数据是否是整形以及无异常值:{} , 错误信息:{}" , ok01 ,error01 ) ;


		    //1.检查坐标系是否CGCS2000
		    string error1 ;
		    bool ok1 = RiskValidateTool::isGeoTiff_CGCS2000(inputFilename,error1) ;
		    spdlog::info("输入数据坐标系是否是CGCS2000:{} , 错误信息:{}" , ok1 ,error1 ) ;


		    //2.检查分辨率是否30秒，如果这一步没通过，不进行第三步检查
		    string error2 ;
		    bool ok2 = RiskValidateTool::isGeoTiff_GridSizeOK(inputFilename,error2) ;
		    spdlog::info("输入数据是否是30秒分辨率:{} , 错误信息:{}" , ok2 ,error2) ;


		    //3.检查行政范围与标准格网是否一致
		    string error3 ;
		    bool ok3 = RiskValidateTool::isGeoTiff_ExtentOk(standardCodeRasterFile,inputFilename,error3);
		    spdlog::info("输入数据对应行政区划是否与标准网格一致:{} , 错误信息:{}" , ok3 ,error3) ;

		    bool outputJsonOk = false;
		    if( ok1==true && ok2==true && ok3==true ){
		    	state = "0";
		        message = "输入数据通过正确性检验";
		        spdlog::info("{} 输入数据通过正确性检验",inputFilename);
		    }else{
		        string outmsg ;
		         if( ok01==false ){
		            //outmsg += "输入数据不是整形或者存在无异常值，详细信息：" + error01 + ";";
		            outmsg+=error01+";" ;
		        }
		        if( ok1==false ){
		            //outmsg += "输入数据不是CGCS2000坐标，详细信息：" + error1 + ";";
		            outmsg+=error1+";" ;
		        }
		        if( ok2==false ){
		            //outmsg += "输入数据分辨率不是30秒，详细信息：" + error2 + ";";
		            outmsg+=error2+";" ;
		        }
		        if( ok3==false ){
		            //outmsg += "输入数据对应行政区划与标准网格不匹配，详细信息：" + error3 + ";";
		            outmsg+=error3+";" ;
		        }
		        state = "9";
		        message = outmsg;
		        spdlog::info("{} 输入数据没有通过检验，错误信息:{}",inputFilename, outmsg);
		    }
    	}else{
    		state = "1";
    		message = "no infile params." ;
    	}

    	message = wStringUtils::replaceString(message,"\"","'") ;
  		res.set_content(
  			string("{\"status:\":")
  			+state
  			+",\"message\":\""
  			+message
  			+"\"}"
  			, 
  			"application/json");
	});
	int portval = atof( port.c_str() ) ;
	svr.listen("0.0.0.0", portval);


	return 0 ;
}

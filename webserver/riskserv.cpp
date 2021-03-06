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
	version += "\nv1.2.2 2022-1-20 add shp extent check, shp class value check, bugfixed for ok01 checking." ;
	version += "\nv1.3.0 2022-1-25 add error detail for tif 02 03 checking." ;
	version += "\nv1.4.1 2022-1-26 use one step check use alg 1.5.0 ." ;

	cout<<"A program to server risk product validate. by wf 2022-1-14."<<endl ;
	cout<<version<<endl ;
	cout<<"usage:riskserv port /some/dir/logfilename debugmode"<<endl ;
	cout<<"debugmode: 0/1 , default is 0."<<endl ;
	cout<<"检验算法版本："<<RiskValidateTool::getVersion() <<endl ;
	cout<<"当前检验算法不对CGCS2000进行判断！！ 2022-1-20"<<endl ;
	GDALAllRegister();
	if( argc!=3 && argc !=4 ){
		cout<<"argc not 3 or 4."<<endl ;
		return 11 ;
	}

	bool debugMode = false ;
	if( argc==4 ){
		debugMode = atof(argv[3]) ;
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
    svr.Get("/validate", [standardCodeRasterFile,debugMode](const httplib::Request &req, httplib::Response &res) {
    	string state = "1" ;// 0-success, 1-do nothing, 9-validate error.
    	string message =  "do nothing" ;
    	if( req.has_param("infile") ){
    		 string inputFilename = req.get_param_value("infile");
    		 spdlog::info("try to check {}" , inputFilename);

    		 string errorall ;
    		 bool allok = RiskValidateTool::checkInOne(standardCodeRasterFile,inputFilename,errorall,debugMode) ;
    		 if( allok==true ){
    		 	state = "0" ;
    		 	message = "输入数据通过正确性检验" ;
    		 }else{
    		 	state = "9" ;
    		 	message = string("输入数据没有通过检验,错误信息:") + errorall ;
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


/*
//根据文件名结尾判断是tif，tiff还是shp
    		 bool fileNameOk = false ;
    		 bool isShp = false ;
    		 string fileNameError ;
    		 string baseName = wStringUtils::getFileNameFromFilePath(inputFilename);
    		 if( baseName.length() < 10 ){
    		 	fileNameError = string("文件名不符合规范:")+baseName;
    		 	fileNameOk = false ;
    		 }else{
    		 	string tail4 = wStringUtils::toLower(baseName.substr( baseName.length() - 4 )) ;//.tif tiff .shp
    		 	if( tail4.compare(".tif") == 0 || tail4.compare("tiff")==0  ){
    		 		//file name is good
    		 		fileNameOk = true ;
    		 		isShp = false ;
    		 	}
    		 	else if(  tail4.compare(".shp")==0 ){
    		 		fileNameOk = true ;
    		 		isShp = true ;
    		 	}
    		 	else{
    		 		fileNameError="文件扩展名不是 .tif .tiff .shp 中的一种";
    		 		fileNameOk = false ;
    		 	}
    		 }

    		 if( fileNameOk==false ){
    		 	spdlog::info(fileNameError) ;
    		 	state = "9";
		        message = fileNameError;
    		 }
    		 else {
    		 	if( isShp==true )
    		 	{
    		 		spdlog::info("it is shp.") ;

    		 		string error1 , error1detail;
    		 		bool ok1 = RiskValidateTool::isShp_ExtentOk(standardCodeRasterFile,inputFilename,error1) ;
    		 		if( ok1==false ) error1detail = RiskValidateTool::getErrorDetail();
    		 		spdlog::info("矢量范围正确:{} , 错误信息:{} . {}" , ok1 ,error1 , RiskValidateTool::getErrorDetail() ) ;

    		 		string error2 ;
    		 		bool ok2 = RiskValidateTool::isShp_classValueOk(inputFilename,error2) ;
    		 		spdlog::info("矢量class值正确:{} , 错误信息:{}" , ok2 ,error2 ) ;

					if( ok1==true && ok2==true ){
				    	state = "0";
				        message = "输入数据通过正确性检验";
				        spdlog::info("{} 输入数据通过正确性检验",inputFilename);
				    }else{
				        string outmsg ;
				        if( ok1==false ){
				            outmsg+=error1+","+ error1detail +";" ;
				        }
				        if( ok2==false ){
				            outmsg+=error2+";" ;
				        }
				        state = "9";
				        message = outmsg;
				        spdlog::info("{} 输入数据没有通过检验，错误信息:{}",inputFilename, outmsg);
				    }
    		 	}else
    		 	{//tif
    		 		spdlog::info("it is tif.") ;
    		 		//0.检查数据类型与取值范围
				    string error01 ;
				    bool ok01 = RiskValidateTool::isGeoTiff_Integer(inputFilename,error01) ;
				    spdlog::info("输入数据是否是整形以及无异常值:{} , 错误信息:{}" , ok01 ,error01 ) ;


				    //1.检查坐标系是否CGCS2000
				    string error1 ;
				    bool ok1 = RiskValidateTool::isGeoTiff_CGCS2000(inputFilename,error1) ;
				    spdlog::info("输入数据坐标系是否是CGCS2000:{} , 错误信息:{}" , ok1 ,error1 ) ;


				    //2.检查分辨率是否30秒，如果这一步没通过，不进行第三步检查
				    string error2  ;
				    bool ok2 = RiskValidateTool::isGeoTiff_GridSizeOK(inputFilename,error2) ;
				    spdlog::info("输入数据是否是30秒分辨率:{} , 错误信息:{}" , ok2 ,error2) ;
				    string detailError2 = RiskValidateTool::getErrorDetail();


				    //3.检查行政范围与标准格网是否一致
				    string error3 ;
				    bool ok3 = RiskValidateTool::isGeoTiff_ExtentOk(standardCodeRasterFile,inputFilename,error3);
				    spdlog::info("输入数据对应行政区划是否与标准网格一致:{} , 错误信息:{},{}" , ok3 ,error3,
				    	RiskValidateTool::getErrorDetail() ) ;
				    string detailError3 = RiskValidateTool::getErrorDetail() ;

				    bool outputJsonOk = false;
				    if( ok01==true && //bugfixed 2022-1-20
				    	 ok1==true && ok2==true && ok3==true ){
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
				            outmsg+=error2+"," + detailError2 + ";" ;
				        }
				        if( ok3==false ){
				            //outmsg += "输入数据对应行政区划与标准网格不匹配，详细信息：" + error3 + ";";
				            outmsg+=error3+"," + detailError3 + ";" ;
				        }
				        state = "9";
				        message = outmsg;
				        spdlog::info("{} 输入数据没有通过检验，错误信息:{}",inputFilename, outmsg);
				    }
    		 	}
    		 }
*/
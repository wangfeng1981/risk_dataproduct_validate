// risk_dataproduct_validate.cpp : This file contains the 'main' function. Program execution begins and ends there.
// 
/// V1.0.0 气象局灾害风险普查提交成果检查程序
///

/// V1.1.0 修改完善输入输出 增加log日志
// input.json
// {
//     "inputfile":"/some/dir/BB_P_01_540321_01.tif",
//     "outputfile":"/some/dir/output.json" 
// }


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

using namespace std;

/// 检验成功status=0
/// 写入json成功返回true，反之返回false
bool writeOutputJson(string outputJsonfilename, string status, string message) {
    ofstream ofs(outputJsonfilename.c_str()) ;
    if( ofs.good()==false ){
        return false ;
    }
    //使用单引号替换双引号，然后写入json
    message = wStringUtils::replaceString(message,"\"","'") ;

    ofs<<"{" ;
    ofs<<"\"status\":\""<<status<<"\"," ;
    ofs<<"\"message\":\""<<message<<"\"" ;
    ofs<<"}" ;
    ofs.close() ;
    return true ;
}

int main(int argc , char* argv[])
{
    string programDescriptionAndVersion ;
    programDescriptionAndVersion += "A program to validate risk product for CMA. 2022-1-7 wangfengdev@163.com\n" ;
    programDescriptionAndVersion += "V1.1.1 2022-1-8\n" ;
    programDescriptionAndVersion += "usage:risk_dataproduct_validate input.tif output.json\n" ;
    programDescriptionAndVersion += "risk_standard_grid_code_V200.tif should be in the same dir of the program. A daily log files in ./logs/date_{yyyy-MM-dd}.log\n" ;
    cout << programDescriptionAndVersion << endl;
    
    GDALAllRegister();

    auto dailylogger = spdlog::daily_logger_mt("l", "./logs/date.log" , 2, 0) ;//2:00 am
    spdlog::flush_every(std::chrono::seconds(1) ) ;
    spdlog::set_default_logger(dailylogger );
    spdlog::info("****************************");
    spdlog::info(programDescriptionAndVersion);
    spdlog::info("检验算法版本：{}", RiskValidateTool::getVersion() );

    cout<<"检验算法版本："<<RiskValidateTool::getVersion() <<endl ;
    if( argc!=3 ){
        cout<<"输入参数错误"<<endl ;
        spdlog::error ("输入参数错误") ;
        return 11 ;
    }

    string exefilename = argv[0] ;
    string inputFilename = argv[1] ;
    string outputJsonFilename = argv[2] ;
    spdlog::info("input file:{}" , inputFilename);
    spdlog::info("output file:{}" , outputJsonFilename);

    string exedir = wDirTools::parentDir(exefilename) ;
    string standardCodeRasterFile = exedir + "/" + "risk_standard_grid_code_V200.tif";
    spdlog::info("try to load {}" , standardCodeRasterFile) ;
    bool standardExist = wDirTools::isFileExist(standardCodeRasterFile) ;
    if( standardExist==false ){
        spdlog::error ("打开标准网格数据文件失败 {}" ,standardCodeRasterFile ) ;
        writeOutputJson(outputJsonFilename,"10","程序内部错误，请检查log文件");
        return 12 ;
    }

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
        outputJsonOk = writeOutputJson(outputJsonFilename,"0","输入数据通过正确性检验");
        spdlog::info("{} 输入数据通过正确性检验",inputFilename);
        cout<<"输入数据通过正确性检验"<<endl ;

    }else{
        string outmsg ;
        if( ok1==false ){
            outmsg += "输入数据不是CGCS2000坐标，详细信息：" + error1 + ";";
        }
        if( ok2==false ){
            outmsg += "输入数据分辨率不是30秒，详细信息：" + error2 + ";";
        }
        if( ok3==false ){
            outmsg += "输入数据对应行政区划与标准网格不匹配，详细信息：" + error3 + ";";
        }
        outputJsonOk = writeOutputJson(outputJsonFilename,"9",outmsg);
        spdlog::info("{} 输入数据没有通过检验，{}",inputFilename, outmsg);
        cout<<"输入数据没有通过检验"<<endl ;
    }



    return 0;
}



#pragma execution_character_set("utf-8")
#pragma once
/// <summary>
/// 产品成果检查工具类，检查逻辑都在这个类里面
/// </summary>
#include <string>
#include "gdal_priv.h"
#include "wGdalRaster.h"
#include <vector>
#include  <memory>
#include "ogrsf_frmts.h"
#include <iostream>
using std::string;
using std::vector;

struct RiskValidateToolPointI {
	int x, y;//col,row
	inline RiskValidateToolPointI() :x(0), y(0) {}
};

class RiskValidateTool
{
public:
	static inline string getVersion() { return version; }

	//2022-1-26 一步全检验
	static bool checkInOne( string standardfilename, string inputfilename, string& error, bool debug ) ;

	
	static bool isGeoTiff_CGCS2000(string rasterfilename, string& error);//unused 2022-1-26
	static bool isShape_CGCS2000(string shpfilename, string& error);//unused 2022-1-26

	static bool isGeoTiff_GridSizeOK(string rasterfilename, string& error);
	static bool isShape_GridSizeOK(string shpfilename, string& error);//unused 2022-1-26

	static bool isGeoTiff_ExtentOk(string standardfilename, string rasterfilename, string& error);
	static bool isShp_ExtentOk(string standardfilename,string shpfilename, string& error);

	//2022-1-9 对整形值的检查，是否是byte,uint16,int16,uint32,int32 危险性0-4 检查 风险0-5检查
	static bool isGeoTiff_Integer(string rasterfilename,string& error) ;
	static bool isShp_classValueOk(string shpfilename,string& error) ;//2022-1-20



	//获取行政编码在标准网格中的范围，行政编码不足9位的，在后面补齐，比如110001，进行查找的时候使用110001000~110001999 2022-1-7
	//行政区划编码不能是0开头的
	static bool computeExtentByCode(string standardGridFilename, int code,
		double& retleft, double& retright, double& rettop, double& retbottom, int& retGridCout,string& error);

	//获取栅格分级数据有效值范围， 计算所有不等于填充值值的有效像素范围
	static bool computeValidExtent(string rasterfilename, 
		double& retleft, double& retright, double& rettop, double& retbottom,int& retValidCout, string& error);

	//逐个网格检查是否匹配地理位置，如果有不匹配像素，最多返回三个输入栅格数据像素位置，从0开始记。
	//栅格数据全部匹配到标准网格返回true，反之返回false
	static bool checkEveryGridMatching(string standardfilename,int code, string rasterfilename,  
		vector<RiskValidateToolPointI>& unmatchingPoints, string& error);

	//输入行政编码计算行政编码下限和上限值
	//行政编码小于0，返回false
	//行政编码长度等于9，low=code，high=code
	//行政编码长度小于9，自动使用0从后面补足9位，e.g. code=123456 code1=123456000 low=123456000 high=123456999
	static bool computeXzCode(int code, int& codelow, int& codehigh);

    //delete static string getErrorDetail(){return errorDetail;}//2022-1-20

private:
	static string double2str(double val) ;
	static string int2str(int val) ;

private:
	static string version;
	static double eps;
    //delete static string errorDetail ;//2022-1-20


};


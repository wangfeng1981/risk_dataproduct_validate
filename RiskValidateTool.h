#pragma once
/// <summary>
/// 产品成果检查工具类，检查逻辑都在这个类里面
/// </summary>
#include <string>
#include "gdal_priv.h"
#include "wGdalRaster.h"
#include <vector>
#include  <memory>
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
	
	static bool isGeoTiff_CGCS2000(string rasterfilename, string& error);
	static bool isShape_CGCS2000(string shpfilename, string& error);

	static bool isGeoTiff_GridSizeOK(string rasterfilename, string& error);
	static bool isShape_GridSizeOK(string shpfilename, string& error);

	static bool isGeoTiff_ExtentOk(string standardfilename, string rasterfilename, string& error);
	static bool isShp_ExtentOk(string shpfilename, string& error);


	//获取行政编码在标准网格中的范围，行政编码不足9位的，在后面补齐，比如110001，进行查找的时候使用110001000~110001999 2022-1-7
	//行政区划编码不能是0开头的
	static bool computeExtentByCode(string standardGridFilename, int code,
		double& retleft, double& retright, double& rettop, double& retbottom, int& retGridCout,string& error);

	//获取栅格分级数据有效值范围，fillValue为填充值，计算所有不等于fillValue值的有效像素范围
	static bool computeValidExtent(string rasterfilename, int fillValue,
		double& retleft, double& retright, double& rettop, double& retbottom,int& retValidCout, string& error);

	//逐个网格检查是否匹配地理位置，如果有不匹配像素，最多返回三个输入栅格数据像素位置，从0开始记。
	//栅格数据全部匹配到标准网格返回true，反之返回false
	static bool checkEveryGridMatching(string standardfilename,int code, string rasterfilename, int rasterfillValue,
		vector<RiskValidateToolPointI>& unmatchingPoints, string& error);

	//输入行政编码计算行政编码下限和上限值
	//行政编码小于0，返回false
	//行政编码长度等于9，low=code，high=code
	//行政编码长度小于9，自动使用0从后面补足9位，e.g. code=123456 code1=123456000 low=123456000 high=123456999
	static bool computeXzCode(int code, int& codelow, int& codehigh);

private:
	static string version;
	static double eps;


};


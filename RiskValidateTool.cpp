#include "RiskValidateTool.h"

#define WGDALRASTER_H_IMPLEMENTATION
#include "wGdalRaster.h"
#include "wstringutils.h"

/// V1.0.0
///   第一个版本 2022-1-7
///   （1）判断CGCS2000坐标的规则为，现将输入数据wkt转成大写，然后前面与"GEOGCS["比较，后面与",AUTHORITY[\"EPSG\",\"4490\"]]"比较，
///     全部相同则认为是CGCS2000坐标。
///   （2）分辨率判断使用10位小数进行判断
/// 
/// 

/// V1.0.1
/// （1） checkEveryGridMatching ，范围判断中返回不匹配点数据最多10个

/// V1.0.2
/// (1)使用精度检验考虑9位小数

/// V1.1.0
/// (1)增加栅格数据类型判断，要求是byte，uint16，int16，uint32，int32 这几种类型，危险性等级取值范围0-4，风险等级取值只能0-5
/// (2)缺少的标准网格的rows和cols录入在错误信息中


string RiskValidateTool::version = "v1.1.0";
double RiskValidateTool::eps = 0.000000001;

bool RiskValidateTool::isGeoTiff_CGCS2000(string rasterfile, string& error)
{
	wGdalRaster* raster = wGdalRasterFactory::OpenFile(rasterfile);
	if (raster == 0) {
		error = "打开文件失败,请检查文件是否是有效的GeoTiff";
		return false;
	}

	string inputWkt = string(raster->getProj()) ;
	inputWkt = wStringUtils::toUpper(inputWkt);

	delete raster;

	
	const string cgcs2000wktHead = "GEOGCS[";
	const string cgcs2000wktTail = ",AUTHORITY[\"EPSG\",\"4490\"]]";

	if (inputWkt.length() < cgcs2000wktHead.length() + cgcs2000wktTail.length()) {
		error = string("输入文件不是标准CGCS2000坐标系(1),输入数据wkt为:") + inputWkt;
		return false;
	}

	string inputWktHead = inputWkt.substr(0, cgcs2000wktHead.length());
	string inputWktTail = inputWkt.substr(inputWkt.length() - cgcs2000wktTail.length());

	if (cgcs2000wktHead.compare(inputWktHead) != 0 || cgcs2000wktTail.compare(inputWktTail) != 0 )
	{
		error = string("输入文件不是标准CGCS2000坐标系(2),输入数据wkt为:") + inputWkt;
		return false;
	}

	return true;
}


bool RiskValidateTool::isShape_CGCS2000(string shpfile, string& error)
{
	return true;
}

bool RiskValidateTool::isGeoTiff_GridSizeOK(string rasterfile, string& error)
{
	wGdalRaster* raster = wGdalRasterFactory::OpenFile(rasterfile);
	if (raster == 0) {
		error = "打开文件失败,请检查文件是否是有效的GeoTiff";
		return false;
	}

	const double* trans = raster->getTrans();
	double transVal[6];
	for (int i = 0; i < 6; ++i) transVal[i] = trans[i];
	delete raster;

	// 30''
	const double thirtySeconds = 30.0 / 3600.0;
	
	bool goodReso = false;
	if (fabs(transVal[2]) < eps && fabs(transVal[4]) < eps)
	{
		if (fabs(thirtySeconds - transVal[1]) < eps && fabs(thirtySeconds + trans[5]) < eps)
		{
			goodReso = true;
		}
	}

	if (goodReso == false) {
		char buffer1[64];
		char buffer2[64];
		sprintf(buffer1, "(%15.10f)", thirtySeconds);
		sprintf(buffer2, "(%15.10f,%15.10f)", transVal[1],transVal[5]);
		error = string("输入栅格数据分辨率不符合30秒") + buffer1 + ",输入数据分辨率:" + buffer2;
		return false;
	}


	return true;
}


bool RiskValidateTool::isShape_GridSizeOK(string shpfile, string& error)
{
	return true;
}

bool RiskValidateTool::isGeoTiff_ExtentOk(string standardfilename,string rasterfilename, string& error)
{
	string fileName = wStringUtils::getFileNameFromFilePath(rasterfilename);
	vector<string> strArr = wStringUtils::splitString(fileName, "_");
	if (strArr.size() < 5) {
		error = string("输入文件文件名不规范:") + fileName + ", 文件名规范为:灾害类别代码_成果类型_数据对象细化代码_6位行政区划编码_序号(01-99).xxx";
		return false;
	}

	string xzcodeStr = strArr[3];
	bool isAllNum = wStringUtils::isAllNumber(xzcodeStr);
	if (isAllNum == false) {
		error = string("行政区划编码无效:") + xzcodeStr;
		return false;
	}
	int xycodei = atof(xzcodeStr.c_str());

	double standleft, standright, standtop, standbottom;
	double inleft, inright, intop, inbottom;
	int standcnt = 0;
	int incnt = 0;

	bool standExtentOk = computeExtentByCode(standardfilename, xycodei, standleft, standright, standtop, standbottom, standcnt, error);
	if (standExtentOk == false) {
		return false;
	}

	bool inExtentOk = computeValidExtent(rasterfilename, 0, inleft, inright, intop, inbottom, incnt, error);
	if (inExtentOk == false) {
		return false;
	}

	if (standcnt != incnt) {
		error = string("输入数据网格数量(")+ wStringUtils::int2str(incnt) + ")与行政区标准网格数量(" 
			+ wStringUtils::int2str(standcnt) +")不匹配" ;
		return false;
	}

	if (fabs(standleft - inleft) > eps || fabs(standright - inright) > eps ||
		fabs(standtop - intop) > eps || fabs(standbottom - inbottom) > eps)
	{
		char buffer1[128];
		char buffer2[128];
		sprintf(buffer1, "%15.10f,%15.10f,%15.10f,%15.10f", inleft, inright, intop, inbottom);
		sprintf(buffer2, "%15.10f,%15.10f,%15.10f,%15.10f", standleft, standright, standtop, standbottom);
		error = string("输入数据空间范围(") + string(buffer1) + ")与行政区标准网格空间范围("
			+ string(buffer2) + ")不匹配";
		return false;
	}

	vector<RiskValidateToolPointI> unMatchPoints;
	bool matchOk = checkEveryGridMatching(standardfilename, xycodei, rasterfilename, 0, unMatchPoints, error);
	if (matchOk == false) {
		return false;
	}

	return true;
}


bool RiskValidateTool::isShp_ExtentOk(string shpfile, string& error)
{
	return true;
}


//获取行政编码在标准网格中的范围，行政编码不足9位的，在后面补齐，比如110001，进行查找的时候使用110001000~110001999 2022-1-7
//行政区划编码不能是0开头的
bool RiskValidateTool::computeExtentByCode(string standardGridFilename, int code,
	double& retleft, double& retright, double& rettop, double& retbottom, int& retGridCout, string& error)
{
	std::shared_ptr<wGdalRaster> standardGridPtr(wGdalRasterFactory::OpenFile(standardGridFilename));
	if (standardGridPtr.get() == 0) {
		error = "打开标准网格数据文件失败";
		return false;
	}

	int codeLow = 0;
	int codeHigh = 9;
	bool codeOk = computeXzCode(code, codeLow, codeHigh);
	if (codeOk ==false ) {
		error = "行政编码无效:" + wStringUtils::int2str(code);
		return false;
	}

	//for debug
	cout << "debug code,low,high:" << code << "," << codeLow << "," << codeHigh << endl;

	retGridCout = 0;
	retleft = 999999.0;
	retright = -999999.0;
	rettop = -999999.0;
	retbottom = 999999.0;

	int xsize = standardGridPtr->getXSize();
	int ysize = standardGridPtr->getYSize();
	const double* trans = standardGridPtr->getTrans();
	for (int iy = 0; iy < ysize; ++iy)
	{
		for (int ix = 0; ix < xsize; ++ix)
		{
			int pxval = standardGridPtr->getValuei(ix, iy, 0);
			if (pxval >= codeLow && pxval <= codeHigh)
			{
				++retGridCout;
				double pxleft = trans[0] + trans[1] * ix;
				double pxtop = trans[3] + trans[5] * iy;
				if (retleft > pxleft) retleft = pxleft;
				if (rettop < pxtop) rettop = pxtop;
			}
		}
	}

	if (retGridCout == 0) {
		error = "标准网格中没有找到行政编码:" + wStringUtils::int2str(codeLow) + "~" + wStringUtils::int2str(codeHigh) ;
		return false;
	}else //if(retGridCout > 0) 
	{
		retright = retleft + trans[1];
		retbottom = rettop + trans[5];
	}
	return true;
}

//获取栅格分级数据有效值范围，fillValue为填充值，计算所有不等于fillValue值的有效像素范围
bool RiskValidateTool::computeValidExtent(string rasterfilename, int fillValue,
	double& retleft, double& retright, double& rettop, double& retbottom, int& retValidCout, string& error)
{
	std::shared_ptr<wGdalRaster> rasterPtr(wGdalRasterFactory::OpenFile(rasterfilename));
	if (rasterPtr.get() == 0) {
		error = "打开输入栅格数据文件失败";
		return false;
	}

	retValidCout = 0;
	retleft = 999999.0;
	retright = -999999.0;
	rettop = -999999.0;
	retbottom = 999999.0;

	int xsize = rasterPtr->getXSize();
	int ysize = rasterPtr->getYSize();
	const double* trans = rasterPtr->getTrans();
	for (int iy = 0; iy < ysize; ++iy)
	{
		for (int ix = 0; ix < xsize; ++ix)
		{
			int pxval = rasterPtr->getValuei(ix, iy, 0);
			if (pxval != fillValue)
			{
				++retValidCout;
				double pxleft = trans[0] + trans[1] * ix;
				double pxtop = trans[3] + trans[5] * iy;
				if (retleft > pxleft) retleft = pxleft;
				if (rettop < pxtop) rettop = pxtop;
			}
		}
	}

	if (retValidCout == 0) {
		error = "输入栅格数据中没有找到有效值" ;
		return false;
	}
	else //if(retGridCout > 0) 
	{
		retright = retleft + trans[1];
		retbottom = rettop + trans[5];
	}
	return true;

}

//逐个网格检查是否匹配地理位置，如果有不匹配像素，最多返回三个输入栅格数据像素位置，从0开始记。
//栅格数据全部匹配到标准网格返回true，反之返回false
bool RiskValidateTool::checkEveryGridMatching(string standardfilename, int code, string rasterfilename, int rasterfillValue,
	vector<RiskValidateToolPointI>& unmatchingPoints, string& error)
{
	std::shared_ptr<wGdalRaster> standardGridPtr(wGdalRasterFactory::OpenFile(standardfilename));
	if (standardGridPtr.get() == 0) {
		error = "打开标准网格数据文件失败";
		return false;
	}

	std::shared_ptr<wGdalRaster> rasterPtr(wGdalRasterFactory::OpenFile(rasterfilename));
	if (rasterPtr.get() == 0) {
		error = "打开输入栅格数据文件失败";
		return false;
	}

	int codeLow = 0;
	int codeHigh = 9;
	bool codeOk = computeXzCode(code, codeLow, codeHigh);
	if (codeOk == false) {
		error = "行政编码无效:" + wStringUtils::int2str(code);
		return false;
	}

	int standxsize = standardGridPtr->getXSize();
	int standysize = standardGridPtr->getYSize();

	int rxsize = rasterPtr->getXSize();
	int rysize = rasterPtr->getYSize();
	const double* trans = rasterPtr->getTrans();
	const double* standtrans = standardGridPtr->getTrans();
	int matchCount = 0;
	int unmatchCount = 0;
	for (int iy = 0; iy < rysize; ++iy)
	{
		for (int ix = 0; ix < rxsize; ++ix)
		{
			int pxval = rasterPtr->getValuei(ix, iy, 0);
			if (pxval != rasterfillValue)
			{
				double pixelcenterx = trans[0] + trans[1] * (ix+0.5) ;
				double pixelcentery = trans[3] + trans[5] * (iy+0.5) ;

				int standx = (pixelcenterx - standtrans[0]) / standtrans[1];
				int standy = (pixelcentery - standtrans[3]) / standtrans[5];

				bool match = false;
				if (standx >= 0 && standx < standxsize && standy >= 0 && standy < standysize)
				{
					int standval = standardGridPtr->getValuei(standx, standy, 0);
					if (standval >= codeLow && standval <= codeHigh)
					{
						++matchCount;
						match = true;
					}
				}

				if (match == false)
				{
					if( unmatchCount < 10 ){
						//避免消耗过多内存，只记录10个不匹配点
						RiskValidateToolPointI unMatchPt;
						unMatchPt.x = ix;
						unMatchPt.y = iy;
						unmatchingPoints.push_back(unMatchPt);
						++unmatchCount;
					}else{
						break ;//多了不做判断了，减少运行时间
					}
					
				}

			}
		}
	}

	if (unmatchCount > 0) {
		error = string("输入栅格数据不在标准网格中格点的数量：") + wStringUtils::int2str(unmatchingPoints.size())+ ",前三个不在标准网格中输入数据格点坐标:";
		for (int iun = 0; iun < unmatchingPoints.size(); ++iun) {
			error += "(" + wStringUtils::int2str(unmatchingPoints[iun].x) + "," + wStringUtils::int2str(unmatchingPoints[iun].y) + ") ";
		}
		return false;
	}

	return true;
}

//输入行政编码计算行政编码下限和上限值
//行政编码小于0，返回false
//行政编码长度等于9，low=code，high=code
//行政编码长度小于9，自动使用0从后面补足9位，e.g. code=123456 code1=123456000 low=123456000 high=123456999
bool RiskValidateTool::computeXzCode(int code, int& codelow, int& codehigh)
{
	if (code <= 0) {
		return false;
	}

	char codeStr[32];
	sprintf(codeStr, "%d", code);
	string codeStr1(codeStr);
	if (codeStr1.length() > 9) {
		return false;
	}

	if (codeStr1.length() == 9)
	{
		codelow = code;
		codehigh = code;
	}

	if (codeStr1.length() < 9)
	{
		codelow = code;
		codehigh = code;
		int temp = 9 - codeStr1.length();
		for (int i = 0; i < temp; ++i)
		{
			codelow = codelow * 10;
			codehigh = codehigh * 10 + 9;
		}
	}
	return true;
}


//2022-1-9 对整形值的检查，是否是byte,uint16,int16,uint32,int32 危险性0-4 检查 风险0-5检查
bool RiskValidateTool::isGeoTiff_Integer(string rasterfilename,string& error) 
{
	string fileName = wStringUtils::getFileNameFromFilePath(rasterfilename);
	vector<string> strArr = wStringUtils::splitString(fileName, "_");
	if (strArr.size() < 5) {
		error = string("输入文件文件名不规范:") + fileName + ", 文件名规范为:灾害类别代码_成果类型_数据对象细化代码_6位行政区划编码_序号(01-99).xxx";
		return false;
	}

	bool pcodeOk = wStringUtils::isAllNumber( strArr[2] ) ;
	if( pcodeOk ==false ){
		error = string("输入文件文件名中数据对象细化代码无效:") +strArr[2];
		return false;
	}

	// 代码01 表示危险性区划 0-4 ， 其他应该都是风险相关 0-5
	int productCode = atof( strArr[2].c_str() ) ;
	int validMin = 0 ;
	int validMax = 4 ;
	if( productCode == 1 ){
		validMax = 4 ;
	}else {
		validMax = 5 ;
	}

	std::shared_ptr<wGdalRaster> rasterPtr(wGdalRasterFactory::OpenFile(rasterfilename));
	if (rasterPtr.get() == 0) {
		error = "打开输入栅格数据文件失败";
		return false;
	}

	GDALDataType dataType = rasterPtr->getDataType();
	if( dataType != GDT_Byte && dataType!= GDT_UInt16 && dataType != GDT_Int16 && dataType != GDT_UInt32 && dataType!= GDT_Int32 )
	{
		error = string("输入栅格数据的数据类型不是整形，请转换为如下类型Byte(推荐),UInt16,Int16,UInt32,Int32重新检验") ;
		return false ;
	}

	int outOfRangePixelCount = 0 ;
	int xsize = rasterPtr->getXSize() ;
	int ysize = rasterPtr->getYSize() ;
	for(int iy = 0; iy < ysize; ++ iy )
	{
		for(int ix = 0 ; ix < xsize; ++ ix )
		{
			int pxval = rasterPtr->getValuei(ix,iy,0) ;
			if( pxval < validMin || pxval > validMax )
			{
				++outOfRangePixelCount;
			}

		}
	}

	if( outOfRangePixelCount>0 ){
		if( productCode==1 ){
			//危险性分级0-4
			error = string("危险性区划（分级）数据取值范围0~4,输入数据超出范围个数:") + wStringUtils::int2str(outOfRangePixelCount) ;
		}
		else{
			//风险分级0-5
			error = string("风险区划（分级）数据取值范围0~5,输入数据超出范围个数:") + wStringUtils::int2str(outOfRangePixelCount) ;
		}
		return false ;
	}
	return true;
}

#include <jni.h>
#include <android/log.h>
#include <time.h>
#include <stdio.h>
#include <vector>
#include <set>
#include <hash_map>
#include <stdlib.h>
#include "SkTypes.h"
#include "SkBitmap.h"
#include "SkShader.h"
#include "SkBitmapProcShader.h"
#include "SkPathEffect.h"
#include "SkBlurDrawLooper.h"
#include "SkDashPathEffect.h"
#include "SkCanvas.h"
#include "SkPaint.h"
#include "SkPath.h"

#include "renderRules.cpp"

JNIEnv* env;

jclass MultiPolygonClass;
jmethodID MultiPolygon_getTag;
jmethodID MultiPolygon_getValue;
jmethodID MultiPolygon_getLayer;
jmethodID MultiPolygon_getPoint31XTile;
jmethodID MultiPolygon_getPoint31YTile;
jmethodID MultiPolygon_getBoundsCount;
jmethodID MultiPolygon_getBoundPointsCount;


jclass BinaryMapDataObjectClass;
jmethodID BinaryMapDataObject_getPointsLength;
jmethodID BinaryMapDataObject_getPoint31YTile;
jmethodID BinaryMapDataObject_getPoint31XTile;
jmethodID BinaryMapDataObject_getTypes;
jmethodID BinaryMapDataObject_getTagValue;

jclass TagValuePairClass;
jfieldID TagValuePair_tag;
jfieldID TagValuePair_value;

jclass RenderingContextClass;

jclass RenderingIconsClass;
jmethodID RenderingIcons_getIcon;

struct IconDrawInfo {
	SkBitmap* bmp;
	float x;
	float y;
};

char debugMessage[1024];

typedef struct RenderingContext {
		jobject originalRC;
		jobject androidContext;
		
		// TODO
		// public boolean interrupted = false;
		// List<TextDrawInfo> textToDraw = new ArrayList<TextDrawInfo>();
		bool highResMode;
		float mapTextSize ;
		float density ;
		std::vector<IconDrawInfo> iconsToDraw;

		float leftX;
		float topY;
		int width;
		int height;

		int zoom;
		float rotate;
		float tileDivisor;

		// debug purpose
		int pointCount ;
		int pointInsideCount;
		int visible;
		int allObjects;

		// use to calculate points
		float calcX;
		float calcY;
		
		float cosRotateTileSize;
		float sinRotateTileSize;
		
		int shadowRenderingMode;
		
		// not expect any shadow
		int shadowLevelMin;
		int shadowLevelMax;

		bool interrupted() {
			// TODO implement !
			return false;
		}
} RenderingContext;



jfieldID getFid(jclass cls,const char* fieldName, const char* sig )
{
	return env->GetFieldID( cls, fieldName, sig);
}


 void calcPoint(jobject mapObject, jint ind, RenderingContext* rc) {
	rc->pointCount++;

	float tx = env->CallIntMethod(mapObject, BinaryMapDataObject_getPoint31XTile, ind) / (rc->tileDivisor);
	float ty = env->CallIntMethod(mapObject, BinaryMapDataObject_getPoint31YTile, ind) / (rc->tileDivisor);

	float dTileX = tx - rc->leftX;
	float dTileY = ty - rc->topY;
	rc->calcX = rc->cosRotateTileSize * dTileX - rc->sinRotateTileSize * dTileY;
	rc->calcY = rc->sinRotateTileSize * dTileX + rc->cosRotateTileSize * dTileY;

	if (rc->calcX >= 0 && rc->calcX < rc->width && rc->calcY >= 0 && rc->calcY < rc->height) {
		rc->pointInsideCount++;
	}
}

 void calcMultipolygonPoint(jobject mapObject, jint ind, jint b, RenderingContext* rc) {
	rc->pointCount++;
	float tx = env->CallIntMethod(mapObject, MultiPolygon_getPoint31XTile, ind, b) / (rc->tileDivisor);
	float ty = env->CallIntMethod(mapObject, MultiPolygon_getPoint31YTile, ind, b) / (rc->tileDivisor);

	float dTileX = tx - rc->leftX;
	float dTileY = ty - rc->topY;
	rc->calcX = rc->cosRotateTileSize * dTileX - rc->sinRotateTileSize * dTileY;
	rc->calcY = rc->sinRotateTileSize * dTileX + rc->cosRotateTileSize * dTileY;

	if (rc->calcX >= 0 && rc->calcX < rc->width && rc->calcY >= 0 && rc->calcY < rc->height) {
		rc->pointInsideCount++;
	}
}

std::hash_map<std::string, SkPathEffect*> pathEffects;
SkPathEffect* getDashEffect(std::string input){
	if(pathEffects.find(input) != pathEffects.end()) {
		return pathEffects[input];
	}
	const char* chars = input.c_str();
	int i = 0;
	char fval[10];
	int flength = 0;
	float primFloats[20];
	int floatLen = 0;
	for(;;i++)
	{
		if(chars[i] == 0)
		{
			if(flength > 0)	{ fval[flength] = 0;
			primFloats[floatLen++] = atof(fval); flength = 0;}
			break;
		}
		else
		{
			if(chars[i] != '_')
			{
				// suppose it is a character
				fval[flength++] = chars[i] ;
			} else {
				if(flength > 0)	{ fval[flength] = 0;
				primFloats[floatLen++] = atof(fval); flength = 0;}
			}
		}
	}
	SkPathEffect* r = new SkDashPathEffect(primFloats, floatLen, 0);
	pathEffects[input] = r;
	return r;
}

SkBitmap* getNativeBitmap(jobject bmpObj){
	if(bmpObj == NULL){
		return NULL;
	}
	jclass bmpClass = env->GetObjectClass(bmpObj);
	SkBitmap* bmp = (SkBitmap*)env->CallIntMethod(bmpObj, env->GetMethodID(bmpClass, "ni", "()I"));
	env->DeleteLocalRef(bmpClass);
	return bmp;
}

std::hash_map<std::string, SkBitmap*> cachedBitmaps;
SkBitmap* getCachedBitmap(RenderingContext* rc, std::string js)
{
	if (cachedBitmaps.find(js) != cachedBitmaps.end()) {
		return cachedBitmaps[js];
	}
	jstring jstr = env->NewStringUTF(js.c_str());
	jobject bmp = env->CallStaticObjectMethod(RenderingIconsClass, RenderingIcons_getIcon, rc->androidContext, jstr);
	SkBitmap* res = getNativeBitmap(bmp);
	env->DeleteLocalRef(bmp);
	env->DeleteLocalRef(jstr);
	if(res != NULL){
		res = new SkBitmap(*res);
	}
	cachedBitmaps[js] = res;
	return res;
}


int updatePaint(RenderingRuleSearchRequest* req, SkPaint* paint, int ind, int area, RenderingContext* rc) {
	RenderingRuleProperty* rColor;
	RenderingRuleProperty* rStrokeW;
	RenderingRuleProperty* rCap;
	RenderingRuleProperty* rPathEff;
	if (ind == 0) {
		rColor = req->props()->R_COLOR;
		rStrokeW = req->props()->R_STROKE_WIDTH;
		rCap = req->props()->R_CAP;
		rPathEff = req->props()->R_PATH_EFFECT;
	} else if (ind == 1) {
		rColor = req->props()->R_COLOR_2;
		rStrokeW = req->props()->R_STROKE_WIDTH_2;
		rCap = req->props()->R_CAP_2;
		rPathEff = req->props()->R_PATH_EFFECT_2;
	} else {
		rColor = req->props()->R_COLOR_3;
		rStrokeW = req->props()->R_STROKE_WIDTH_3;
		rCap = req->props()->R_CAP_3;
		rPathEff = req->props()->R_PATH_EFFECT_3;
	}
	if (area) {
		paint->setStyle(SkPaint::kStrokeAndFill_Style);
		paint->setStrokeWidth(0);
	} else {
		float stroke = req->getFloatPropertyValue(rStrokeW);
		if (!(stroke > 0)) {
			return 0;
		}

		paint->setStyle(SkPaint::kStroke_Style);
		paint->setStrokeWidth(stroke);
		std::string cap = req->getStringPropertyValue(rCap);
		std::string pathEff = req->getStringPropertyValue(rPathEff);

		if (cap == "BUTT" || cap == "") {
			paint->setStrokeCap(SkPaint::kButt_Cap);
		} else if (cap == "ROUND") {
			paint->setStrokeCap(SkPaint::kRound_Cap);
		} else if (cap == "SQUARE") {
			paint->setStrokeCap(SkPaint::kSquare_Cap);
		} else {
			paint->setStrokeCap(SkPaint::kButt_Cap);
		}

		if (pathEff.size() > 0) {
			SkPathEffect* p = getDashEffect(pathEff);
			paint->setPathEffect(p);
		} else {
			paint->setPathEffect(NULL);
		}

	}

	int color = req->getIntPropertyValue(rColor);
	paint->setColor(color);

	if (ind == 0) {
		std::string shader = req->getStringPropertyValue(req->props()->R_SHADER);
		if (shader.size() > 0) {
			SkBitmap* bmp = getCachedBitmap(rc, shader);
			if (bmp == NULL) {
				paint->setShader(NULL);
			} else {
				paint->setShader(new SkBitmapProcShader(*bmp, SkShader::kRepeat_TileMode, SkShader::kRepeat_TileMode))->unref();
			}
		} else {
			paint->setShader(NULL);
		}
	} else {
		paint->setShader(NULL);
	}

	// do not check shadow color here
	if (rc->shadowRenderingMode != 1 || ind != 0) {
		paint->setLooper(NULL);
	} else {
		int shadowColor = req->getIntPropertyValue(req->props()->R_SHADOW_COLOR);
		int shadowLayer = req->getIntPropertyValue(req->props()->R_SHADOW_RADIUS);
		if (shadowColor == 0) {
			shadowLayer = 0;
		}
		if (shadowLayer > 0) {
			paint->setLooper(new SkBlurDrawLooper(shadowLayer, 0, 0, shadowColor))->unref();
		}
		paint->setLooper(NULL);
	}
	return 1;
}

void drawPolylineShadow(SkCanvas* cv, SkPaint* paint, RenderingContext* rc, SkPath* path, int shadowColor,
		int shadowRadius) {
	// blurred shadows
	if (rc->shadowRenderingMode == 2 && shadowRadius > 0) {
		// simply draw shadow? difference from option 3 ?
		// paint.setColor(shadowRadius);
		// paint.setColor(0xffffffff);
		paint->setLooper(new SkBlurDrawLooper(shadowRadius, 0, 0, shadowColor))->unref();
		cv->drawPath(*path, *paint);
	}

	// option shadow = 3 with solid border
	if (rc->shadowRenderingMode == 3 && shadowRadius > 0) {
		paint->setLooper(NULL);
		paint->setStrokeWidth(paint->getStrokeWidth() + shadowRadius * 2);
		paint->setColor(0xffbababa);
//			paint.setColor(shadowColor);
		cv->drawPath(*path, *paint);
	}
}

 void drawPolyline(jobject binaryMapDataObject,	RenderingRuleSearchRequest* req, SkCanvas* cv, SkPaint* paint,
 		RenderingContext* rc, jobject pair, int layer, int drawOnlyShadow)
 {
	if (req == NULL || pair == NULL) {
		return;
	}
	jint length = env->CallIntMethod( binaryMapDataObject,
			BinaryMapDataObject_getPointsLength);
	if (length < 2) {
		return;
	}
	std::string tag = getStringField(pair, TagValuePair_tag);
	std::string value = getStringField(pair, TagValuePair_value);

	req->setInitialTagValueZoom(tag, value, rc->zoom);
	req->setIntFilter(req->props()->R_LAYER, layer);
	// TODO oneway
	// int oneway = 0;
	//if(rc -> zoom >= 16 && "highway".equals(pair.tag) && MapRenderingTypes.isOneWayWay(obj.getHighwayAttributes())){
	//strcmp("highway") oneway = 1;
	//}

	bool rendered = req->searchRule(2);
	if (!rendered || !updatePaint(req,paint, 0, 0, rc)) {
		return;
	}

	rc->visible++;
	SkPath path ;
	int i = 0;
	// TODO calculate text
	for (; i < length; i++) {
		calcPoint(binaryMapDataObject, i, rc);
		if (i == 0) {
			path.moveTo(rc->calcX, rc->calcY);
		} else {
			path.lineTo(rc->calcX, rc->calcY);
		}
	}
	if (i > 0) {
		if (drawOnlyShadow) {
			int shadowColor = req->getIntPropertyValue(req->props()->R_SHADOW_COLOR);
			int shadowRadius = req->getIntPropertyValue(req->props()->R_SHADOW_RADIUS);
			drawPolylineShadow(cv, paint, rc, &path, shadowColor, shadowRadius);
		} else {
			cv->drawPath(path, *paint);
			if (updatePaint(req, paint, 1, 0, rc)) {
				cv->drawPath(path, *paint);
				if (updatePaint(req, paint, 2, 0, rc)) {
					cv->drawPath(path, *paint);
				}
			}
			// TODO
//			if (oneway && !drawOnlyShadow) {
//				Paint[] paints = getOneWayPaints();
//				for (int i = 0; i < paints.length; i++) {
//					canvas.drawPath(path, paints[i]);
//				}
//			}
//			if (!drawOnlyShadow && obj.getName() != null && obj.getName().length() > 0) {
//				calculatePolylineText(obj, render, rc, pair, path, pathRotate, roadLength, inverse, xMid, yMid,
//						middlePoint);
//			}
		}
	}
}

void drawMultiPolygon(jobject binaryMapDataObject,RenderingRuleSearchRequest* req, SkCanvas* cv, SkPaint* paint,
 		  		RenderingContext* rc) {
	if (req == NULL) {
		return;
	}
	std::string tag = getStringMethod(binaryMapDataObject, MultiPolygon_getTag);
	std::string value = getStringMethod(binaryMapDataObject, MultiPolygon_getValue);

	req->setInitialTagValueZoom(tag, value, rc->zoom);
	bool rendered = req->searchRule(3);

	if (!rendered || !updatePaint(req, paint, 0, 1, rc)) {
		return;
	}

	int boundsCount = env->CallIntMethod(binaryMapDataObject, MultiPolygon_getBoundsCount);
	rc->visible++;
	SkPath path;

	for (int i = 0; i < boundsCount; i++) {
		int cnt = env->CallIntMethod(binaryMapDataObject, MultiPolygon_getBoundPointsCount, i);
		float xText = 0;
		float yText = 0;
		for (int j = 0; j < cnt; j++) {
			calcMultipolygonPoint(binaryMapDataObject, j, i, rc);
			xText += rc->calcX;
			yText += rc->calcY;
			if (j == 0) {
				path.moveTo(rc->calcX, rc->calcY);
			} else {
				path.lineTo(rc->calcX, rc->calcY);
			}
		}
		if (cnt > 0) {
			// TODO name
			// String name = ((MultyPolygon) obj).getName(i);
			// if (name != null) {
			// drawPointText(render, rc, new TagValuePair(tag, value), xText / cnt, yText / cnt, name);
			// }
		}
	}

	cv->drawPath(path, *paint);
	// for test purpose
	// render.strokeWidth = 1.5f;
	// render.color = Color.BLACK;
	if (updatePaint(req, paint, 1, 0, rc)) {
		cv->drawPath(path, *paint);
	}
}

void drawPolygon(jobject binaryMapDataObject, RenderingRuleSearchRequest* req, SkCanvas* cv, SkPaint* paint,
		  		RenderingContext* rc, jobject pair) {
	if (req == NULL || pair == NULL) {
		return;
	}
	jint length = env->CallIntMethod(binaryMapDataObject, BinaryMapDataObject_getPointsLength);
	if (length <= 2) {
		return;
	}
	std::string tag = getStringField(pair, TagValuePair_tag);
	std::string value = getStringField(pair, TagValuePair_value);

	req->setInitialTagValueZoom(tag, value, rc->zoom);
	bool rendered = req->searchRule(3);

	float xText = 0;
	float yText = 0;
	if (!rendered || !updatePaint(req, paint, 0, 1, rc)) {
		return;
	}

	rc->visible++;
	SkPath path;
	int i = 0;
	float px = 0;
	float py = 0;
	for (; i < length; i++) {
		calcPoint(binaryMapDataObject, i, rc);
		if (i == 0) {
			path.moveTo(rc->calcX, rc->calcY);
		} else {
			path.lineTo(rc->calcX, rc->calcY);
		}
		xText += px;
		yText += py;
	}

	cv->drawPath(path, *paint);
	if (updatePaint(req, paint, 1, 0, rc)) {
		cv->drawPath(path, *paint);
	}
	// TODO polygon text
//			String name = obj.getName();
//			if(name != null){
//				drawPointText(render, rc, pair, xText / len, yText / len, name);
//			}
//		}
}

void drawPoint(jobject binaryMapDataObject,	RenderingRuleSearchRequest* req, SkCanvas* cv, SkPaint* paint,
 		RenderingContext* rc, jobject pair, int renderText)
 {
	if (req == NULL || pair == NULL) {
		return;
	}

	std::string tag = getStringField(pair, TagValuePair_tag);
	std::string value = getStringField(pair, TagValuePair_value);

	req->setInitialTagValueZoom(tag, value, rc->zoom);
	req->searchRule(1);
	std::string resId = req->getStringPropertyValue(req-> props()-> R_ICON);
	SkBitmap* bmp = getCachedBitmap(rc, resId);
	jstring name = NULL;
	if (renderText) {
		// TODO text
//		name = obj.getName();
	}
	if (!bmp && !name) {
		return;
	}

	jint length = env->CallIntMethod(binaryMapDataObject, BinaryMapDataObject_getPointsLength);
	rc->visible++;
	float px = 0;
	float py = 0;
	int i = 0;
	for (; i < length; i++) {
		calcPoint(binaryMapDataObject, i, rc);
		px += rc->calcX;
		py += rc->calcY;
	}
	if (length > 1) {
		px /= length;
		py /= length;
	}

	if (bmp != NULL) {
		IconDrawInfo ico;
		ico.x = px;
		ico.y = py;
		ico.bmp = bmp;
		rc->iconsToDraw.push_back(ico);
	}
	if (name != NULL) {
		// TODO text
//		drawPointText(render, rc, pair, px, py, name);
	}

}

// 0 - normal, -1 - under, 1 - bridge,over
int getNegativeWayLayer(int type) {
	int i = (3 & (type >> 12));
	if (i == 1) {
		return -1;
	} else if (i == 2) {
		return 1;
	}
	return 0;
}

void drawObject(RenderingContext* rc, jobject binaryMapDataObject, SkCanvas* cv, RenderingRuleSearchRequest* req,
		SkPaint* paint, int l, int renderText, int drawOnlyShadow) {
	rc->allObjects++;
	if (env->IsInstanceOf(binaryMapDataObject, MultiPolygonClass)) {
		if (!drawOnlyShadow) {
			drawMultiPolygon(binaryMapDataObject, req, cv, paint, rc);
		}
		return;
	}

	jintArray types = (jintArray) env->CallObjectMethod(binaryMapDataObject, BinaryMapDataObject_getTypes);
	jint mainType;
	env->GetIntArrayRegion(types, l, 1, &mainType);
	int t = mainType & 3;
	env->DeleteLocalRef(types);

	jobject pair = env->CallObjectMethod(binaryMapDataObject, BinaryMapDataObject_getTagValue, l);
	if (t == 1 && !drawOnlyShadow) {
		// point
		drawPoint(binaryMapDataObject, req, cv, paint, rc, pair, renderText);
	} else if (t == 2) {
		// polyline
		int layer = getNegativeWayLayer(mainType);
//			__android_log_print(ANDROID_LOG_WARN, "net.osmand", "Draw polyline");
		drawPolyline(binaryMapDataObject, req, cv, paint, rc, pair, layer, drawOnlyShadow);
	} else if (t == 3 && !drawOnlyShadow) {
		// polygon
		drawPolygon(binaryMapDataObject, req, cv, paint, rc, pair);
	}
	env->DeleteLocalRef(pair);
}




void copyRenderingContext(jobject orc, RenderingContext* rc) 
{
	rc->leftX = env->GetFloatField( orc, getFid( RenderingContextClass, "leftX", "F" ) );
	rc->topY = env->GetFloatField( orc, getFid( RenderingContextClass, "topY", "F" ) );
	rc->width = env->GetIntField( orc, getFid( RenderingContextClass, "width", "I" ) );
	rc->height = env->GetIntField( orc, getFid( RenderingContextClass, "height", "I" ) );
	
	
	rc->zoom = env->GetIntField( orc, getFid( RenderingContextClass, "zoom", "I" ) );
	rc->rotate = env->GetFloatField( orc, getFid( RenderingContextClass, "rotate", "F" ) );
	rc->tileDivisor = env->GetFloatField( orc, getFid( RenderingContextClass, "tileDivisor", "F" ) );
	
	rc->pointCount = env->GetIntField( orc, getFid( RenderingContextClass, "pointCount", "I" ) );
	rc->pointInsideCount = env->GetIntField( orc, getFid( RenderingContextClass, "pointInsideCount", "I" ) );
	rc->visible = env->GetIntField( orc, getFid( RenderingContextClass, "visible", "I" ) );
	rc->allObjects = env->GetIntField( orc, getFid( RenderingContextClass, "allObjects", "I" ) );
	
	rc->cosRotateTileSize = env->GetFloatField( orc, getFid( RenderingContextClass, "cosRotateTileSize", "F" ) );
	rc->sinRotateTileSize = env->GetFloatField( orc, getFid( RenderingContextClass, "sinRotateTileSize", "F" ) );
	rc->density = env->GetFloatField( orc, getFid( RenderingContextClass, "density", "F" ) );
	rc->highResMode = env->GetBooleanField( orc, getFid( RenderingContextClass, "highResMode", "Z" ) );
	rc->mapTextSize = env->GetFloatField( orc, getFid( RenderingContextClass, "mapTextSize", "F" ) );


	rc->shadowRenderingMode = env->GetIntField( orc, getFid( RenderingContextClass, "shadowRenderingMode", "I" ) );
	rc->shadowLevelMin = env->GetIntField( orc, getFid( RenderingContextClass, "shadowLevelMin", "I" ) );
	rc->shadowLevelMax = env->GetIntField( orc, getFid( RenderingContextClass, "shadowLevelMax", "I" ) );
	rc->androidContext = env->GetObjectField(orc, getFid( RenderingContextClass, "ctx", "Landroid/content/Context;"));
	
	rc->originalRC = orc; 

}


void mergeRenderingContext(jobject orc, RenderingContext* rc) 
{
	env->SetIntField( orc, getFid(RenderingContextClass, "pointCount", "I" ) , rc->pointCount);
	env->SetIntField( orc, getFid(RenderingContextClass, "pointInsideCount", "I" ) , rc->pointInsideCount);
	env->SetIntField( orc, getFid(RenderingContextClass, "visible", "I" ) , rc->visible);
	env->SetIntField( orc, getFid(RenderingContextClass, "allObjects", "I" ) , rc->allObjects);
	env->DeleteLocalRef(rc->androidContext);

}

void initLibrary(jobject rc)
{
   MultiPolygonClass = globalRef(env->FindClass( "net/osmand/osm/MultyPolygon"));
   MultiPolygon_getTag = env->GetMethodID( MultiPolygonClass, "getTag", "()Ljava/lang/String;" );
   MultiPolygon_getValue = env->GetMethodID( MultiPolygonClass, "getValue", "()Ljava/lang/String;" );
   MultiPolygon_getLayer = env->GetMethodID( MultiPolygonClass, "getLayer", "()I" );
   MultiPolygon_getPoint31XTile =env->GetMethodID( MultiPolygonClass, "getPoint31XTile", "(II)I" );
   MultiPolygon_getPoint31YTile =env->GetMethodID( MultiPolygonClass, "getPoint31YTile", "(II)I" );
   MultiPolygon_getBoundsCount =env->GetMethodID( MultiPolygonClass, "getBoundsCount", "()I" );
   MultiPolygon_getBoundPointsCount =env->GetMethodID( MultiPolygonClass, "getBoundPointsCount", "(I)I" );

   RenderingContextClass = globalRef(env->GetObjectClass( rc));

   RenderingIconsClass = globalRef(env->FindClass( "net/osmand/plus/render/RenderingIcons"));
   RenderingIcons_getIcon = env->GetStaticMethodID(RenderingIconsClass, "getIcon","(Landroid/content/Context;Ljava/lang/String;)Landroid/graphics/Bitmap;");

   BinaryMapDataObjectClass = globalRef(env->FindClass( "net/osmand/binary/BinaryMapDataObject"));
   BinaryMapDataObject_getPointsLength = env->GetMethodID( BinaryMapDataObjectClass,"getPointsLength","()I");
   BinaryMapDataObject_getPoint31YTile = env->GetMethodID( BinaryMapDataObjectClass,"getPoint31YTile","(I)I");
   BinaryMapDataObject_getPoint31XTile = env->GetMethodID( BinaryMapDataObjectClass,"getPoint31XTile","(I)I");
   BinaryMapDataObject_getTypes = env->GetMethodID( BinaryMapDataObjectClass,"getTypes","()[I");
   BinaryMapDataObject_getTagValue = env->GetMethodID( BinaryMapDataObjectClass,"getTagValue",
		   "(I)Lnet/osmand/binary/BinaryMapIndexReader$TagValuePair;");

   TagValuePairClass = globalRef(env->FindClass( "net/osmand/binary/BinaryMapIndexReader$TagValuePair"));
   TagValuePair_tag = env->GetFieldID( TagValuePairClass, "tag", "Ljava/lang/String;");
   TagValuePair_value= env->GetFieldID( TagValuePairClass, "value", "Ljava/lang/String;");

}

void unloadLibrary()
{
   env->DeleteGlobalRef( MultiPolygonClass );
   env->DeleteGlobalRef( RenderingContextClass );
   env->DeleteGlobalRef( RenderingIconsClass );
   env->DeleteGlobalRef( BinaryMapDataObjectClass );

}

float getDensityValue(RenderingContext* rc, float val) {
	if (rc -> highResMode && rc -> density > 1) {
		return val * rc -> density * rc -> mapTextSize;
	} else {
		return val * rc -> mapTextSize;
	}
}


void drawIconsOverCanvas(RenderingContext* rc, SkCanvas* canvas)
{
	int skewConstant = (int) getDensityValue(rc, 16);
	int iconsW = rc -> width / skewConstant;
	int iconsH = rc -> height / skewConstant;
	int len = (iconsW * iconsH) / 32;
	int alreadyDrawnIcons[len];
	memset(alreadyDrawnIcons, 0, sizeof(int)*len);
	size_t ji = 0;
	SkPaint p;
	p.setStyle(SkPaint::kStroke_Style);
	for(;ji< rc->iconsToDraw.size(); ji++)
	{
		IconDrawInfo icon = rc->iconsToDraw.at(ji);
		if (icon.y >= 0 && icon.y < rc -> height && icon.x >= 0 && icon.x < rc -> width &&
				icon.bmp != NULL) {
			int z = (((int) icon.x / skewConstant) + ((int) icon.y / skewConstant) * iconsW);
			int i = z / 32;
			if (i >= len) {
				continue;
			}
			int ind = alreadyDrawnIcons[i];
			int b = z % 32;
			// check bit b if it is set
			if (((ind >> b) & 1) == 0) {
				alreadyDrawnIcons[i] = ind | (1 << b);
				SkBitmap* ico = icon.bmp;
				canvas->drawBitmap(*ico, icon.x - ico->width() / 2, icon.y - ico->height() / 2, &p);
			}
		}
		if(rc->interrupted()){
			return;
		}
	}
}

std::hash_map<int, std::vector<int> > sortObjectsByProperOrder(jobjectArray binaryMapDataObjects,
			RenderingRuleSearchRequest* req, RenderingContext* rc) {
	std::hash_map<int, std::vector<int> > orderMap;
	if (req != NULL) {
		req->clearState();
		const size_t size = env->GetArrayLength(binaryMapDataObjects);
		size_t i = 0;
		for (; i < size; i++) {
			uint sh = i << 8;
			jobject binaryMapDataObject = (jobject) env->GetObjectArrayElement(binaryMapDataObjects, i);
			if (env->IsInstanceOf(binaryMapDataObject, MultiPolygonClass)) {
				int layer = env->CallIntMethod(binaryMapDataObject, MultiPolygon_getLayer);
				std::string tag = getStringMethod(binaryMapDataObject, MultiPolygon_getTag);
				std::string value = getStringMethod(binaryMapDataObject, MultiPolygon_getValue);

				req->setTagValueZoomLayer(tag, value, rc->zoom, layer);
				req->setIntFilter(req->props()->R_ORDER_TYPE, RenderingRulesStorage::POLYGON_RULES);
				if (req->searchRule(RenderingRulesStorage::ORDER_RULES)) {
					int order = req->getIntPropertyValue(req->props()->R_ORDER);
					orderMap[order].push_back(sh);
					if (req->getIntPropertyValue(req->props()->R_SHADOW_LEVEL) > 0) {
						rc->shadowLevelMin = std::min(rc->shadowLevelMin, order);
						rc->shadowLevelMax = std::max(rc->shadowLevelMax, order);
						req->clearIntvalue(req->props()->R_SHADOW_LEVEL);
					}
				}
			} else {
				jintArray types = (jintArray) env->CallObjectMethod(binaryMapDataObject, BinaryMapDataObject_getTypes);
				if (types != NULL) {
					jint sizeTypes = env->GetArrayLength(types);
					jint* els = env->GetIntArrayElements(types, NULL);
					int j = 0;
					for (; j < sizeTypes; j++) {
						int wholeType = els[j];
						int mask = wholeType & 3;
						int layer = 0;
						if (mask != 1) {
							layer = getNegativeWayLayer(wholeType);
						}
						jobject pair = env->CallObjectMethod(binaryMapDataObject, BinaryMapDataObject_getTagValue, j);
						if (pair != NULL) {
							std::string tag = getStringField(pair, TagValuePair_tag);
							std::string value = getStringField(pair, TagValuePair_value);
							req->setTagValueZoomLayer(tag, value, rc->zoom, layer);
							req->setIntFilter(req->props()->R_ORDER_TYPE, mask);
							if (req->searchRule(RenderingRulesStorage::ORDER_RULES)) {
								int order = req->getIntPropertyValue(req->props()->R_ORDER);
								orderMap[order].push_back(sh + j);
								if(req->getIntPropertyValue(req->props()->R_SHADOW_LEVEL) > 0) {
									rc->shadowLevelMin = std::min(rc->shadowLevelMin, order);
									rc->shadowLevelMax = std::max(rc->shadowLevelMax, order);
									req->clearIntvalue(req->props()->R_SHADOW_LEVEL);
								}
							}
							env->DeleteLocalRef(pair);
						}
					}
					env->ReleaseIntArrayElements(types, els, JNI_ABORT);
					env->DeleteLocalRef(types);

				}
			}
			env->DeleteLocalRef(binaryMapDataObject);
		}
	}
	return orderMap;
}

int objCount = 0;
void objectDrawn(bool notify = false)
{
	if (objCount++ > 25) {
		// TODO notification
		//notifyListeners(notifyList);
		objCount = 0;
	}
}

void doRendering(jobjectArray binaryMapDataObjects, SkCanvas* canvas, SkPaint* paint,
		RenderingRuleSearchRequest* req, RenderingContext* rc) {
	// put in order map
	std::hash_map<int, std::vector<int> > orderMap = sortObjectsByProperOrder(binaryMapDataObjects, req, rc);
	std::set<int> keys;
	std::hash_map<int, std::vector<int> >::iterator it = orderMap.begin();
	while(it != orderMap.end())
	{
		keys.insert(it->first);
		it++;
	}
	bool shadowDrawn = false;

	for (std::set<int>::iterator ks = keys.begin(); ks != keys.end() ; ks++) {
		if (!shadowDrawn && *ks >= rc->shadowLevelMin && *ks <= rc->shadowLevelMax &&
				rc->shadowRenderingMode > 1) {
			for (std::set<int>::iterator ki = ks; ki != keys.end() ; ki++) {
				if (*ki > rc->shadowLevelMax || rc->interrupted()) {
					break;
				}
				std::vector<int> list = orderMap[*ki];
				for (std::vector<int>::iterator ls = list.begin(); ls != list.end(); ls++) {
					int i = *ls;
					int ind = i >> 8;
					int l = i & 0xff;
					jobject binaryMapDataObject = (jobject) env->GetObjectArrayElement(binaryMapDataObjects, ind);

					// show text only for main type
					drawObject(rc, binaryMapDataObject, canvas, req, paint, l, l == 0, true);
					objectDrawn();
					env->DeleteLocalRef(binaryMapDataObject);
				}
			}
			shadowDrawn = true;
		}

		std::vector<int> list = orderMap[*ks];
		for (std::vector<int>::iterator ls = list.begin(); ls != list.end(); ls++) {
			int i = *ls;
			int ind = i >> 8;
			int l = i & 0xff;

			jobject binaryMapDataObject = (jobject) env->GetObjectArrayElement(binaryMapDataObjects, ind);

			// show text only for main type
			drawObject(rc, binaryMapDataObject, canvas, req, paint, l, l == 0, false);
			objCount++;
			env->DeleteLocalRef(binaryMapDataObject);
			objectDrawn();

		}
		if (rc->interrupted()) {
			return;
		}

	}

	objectDrawn(true);
	drawIconsOverCanvas(rc, canvas);


	objectDrawn(true);
	// TODO text draw
//	drawTextOverCanvas(rc, cv, useEnglishNames);


}


#ifdef __cplusplus
extern "C" {
#endif
JNIEXPORT jstring JNICALL Java_net_osmand_plus_render_NativeOsmandLibrary_generateRendering( JNIEnv* ienv,
		jobject obj, jobject renderingContext, jobjectArray binaryMapDataObjects, jobject bmpObj,
		jboolean useEnglishNames, jobject renderingRuleSearchRequest, jint defaultColor) {
	__android_log_print(ANDROID_LOG_WARN, "net.osmand", "Initializing rendering");
	if(!env) {
	   env = ienv;
	   initLibrary(renderingContext);
	   initRenderingRules(env, renderingRuleSearchRequest);
	}

	SkPaint* paint = new SkPaint;
	paint->setAntiAlias(true);

	SkBitmap* bmp = getNativeBitmap(bmpObj);
	SkCanvas* canvas = new SkCanvas(*bmp);
	sprintf(debugMessage, "Image w:%d h:%d !", bmp->width(), bmp->height());
	__android_log_print(ANDROID_LOG_WARN, "net.osmand", debugMessage);
	canvas->drawColor(defaultColor);

	RenderingRuleSearchRequest* req =  initSearchRequest(renderingRuleSearchRequest);

    RenderingContext rc;
    copyRenderingContext(renderingContext, &rc);
    __android_log_print(ANDROID_LOG_WARN, "net.osmand", "Rendering image");
    
    doRendering(binaryMapDataObjects, canvas, paint, req, &rc);

    __android_log_print(ANDROID_LOG_WARN, "net.osmand", "End Rendering image");
    delete paint;
    delete canvas;
    delete req;
  	mergeRenderingContext(renderingContext, &rc);

    sprintf(debugMessage, "Native ok.");
    jstring result = env->NewStringUTF( debugMessage);
//  unloadLibrary();
	return result;
}

#ifdef __cplusplus
}
#endif

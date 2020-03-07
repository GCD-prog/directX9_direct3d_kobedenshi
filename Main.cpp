#include<windows.h>
#include<stdio.h>
#include<d3dx9.h>
#include<time.h>

#pragma comment(lib, "d3d9.lib")
#pragma comment(lib, "d3dx9.lib")
#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "winmm.lib")

#define	FULLSCREEN	0		// フルスクリーン時に 1 にする
#define	SCRW		640		// Width
#define	SCRH		480		// Height

LPDIRECT3D9				lpD3D;		// Direct3Dインターフェイス
LPDIRECT3DDEVICE9		lpD3DDevice;	// Direct3DDeviceインターフェイス
D3DPRESENT_PARAMETERS d3dpp;

#define ZNUM 2000	//ビルボードの数

#define GNUM 20		//後方視点のカメラの遅れ具合
//#define SPEED 3		//自機の速度

LPD3DXFONT lpFont = NULL; //文字フォント

LPDIRECT3DTEXTURE9 lpSea;	//海面テクスチャ
LPDIRECT3DTEXTURE9 lpFlash;	//発射光テクスチャ
LPDIRECT3DTEXTURE9 lpGun;	//機銃テクスチャ


//自前でZｿｰﾄするための構造体
struct ZSORT{
	void *p;	//そのビルボードに必要な構造体へのポインタ(NULL:未使用)
	float Z;	//Z座標
	void (*RenderProc)(void *);
	int (*UpdateProc)(void *,float *);
};

struct ZSORT ZSort[ZNUM];
int ZCnt;

// 今回使用する頂点フォーマットの定義
struct VERTEX
{
	D3DXVECTOR3 Pos;
	D3DCOLOR color;
	D3DXVECTOR2 Tex;
};

//機銃用 構造体
struct PHAL{
	D3DXMATRIX Mat;
	D3DXVECTOR3 Pos;
	D3DXVECTOR3 OldPos;
	int Flg;
};

#define	FVF_VERTEX (D3DFVF_XYZ | D3DFVF_DIFFUSE | D3DFVF_TEX1)

typedef struct _RECTF
{
	float left;
	float top;
	float right;
	float bottom;
}RECTF;

//メッシュ管理用 構造体
struct XFILE{
	LPD3DXMESH lpMesh;
	DWORD NumMaterial;
	D3DMATERIAL9 *Mat;
	LPDIRECT3DTEXTURE9 *Tex;
};

//各種メッシュ用
struct XFILE Plane;	//自機
struct XFILE Sky;	//空

//自機のヨー・ピッチ・ロールの勢い
float Roll,Pitch,Yaw;

//カメラの向き（ビルボードのために使用）
float RotX,RotY;

//カメラ関連
D3DXVECTOR3 GhostPos;	//カメラの注視座標
D3DXVECTOR3 CameraPos;	//カメラの座標
D3DXVECTOR3 HeadVec;	//カメラの頭上方向

//自機の状態を表す行列
D3DXMATRIX mPlane;

//自機の状態の履歴 格納用
D3DXMATRIX mGhost[GNUM];

//現在のカメラの逆行列
D3DXMATRIX mRevCameraRot;

//機銃の発射間隔
int FireTime;

float Speed;

//スペースキー連射防止フラグ
int SpaceKeyFlg;
int KeyFlg;

void ZSortInit(void *p,void (*ppRenderProc)(void *),int (*ppUpdateProc)(void *,float *),D3DXVECTOR3 Pos)
{
	//一度だけUpdateProcを実行し、初期位置におけるカメラZ位置を算出しておく
	float Z;
	ppUpdateProc(p,&Z);

	if(ZCnt<ZNUM){
		ZSort[ZCnt].p=p;
		ZSort[ZCnt].RenderProc=ppRenderProc;
		ZSort[ZCnt].UpdateProc=ppUpdateProc;

		ZSort[ZCnt].Z=Z;

		ZCnt++;
	}else{
		//ビルボードが足りなければ
		//もっとも遠いものを削除
		free(ZSort[0].p);

		//消した部分に新登場
		ZSort[0].p=p;
		ZSort[0].RenderProc=ppRenderProc;
		ZSort[0].UpdateProc=ppUpdateProc;

		ZSort[0].Z=Z;
	}
}

// 機銃レンダリング
void RenderPhal(void *p_void)
{
	struct PHAL *p=(struct PHAL *)p_void;

	VERTEX v[4];
	float Len;
	Len=D3DXVec3Length(&(p->OldPos-p->Pos));

	D3DXVECTOR3 Cross;

	D3DXVec3Cross(&Cross,&(p->OldPos-p->Pos),&(CameraPos-p->Pos));
	D3DXVec3Normalize(&Cross,&Cross);
	Cross*=0.5f;

	v[0].Pos=p->Pos-Cross;
	v[1].Pos=p->Pos+Cross;
	v[2].Pos=p->OldPos+Cross;
	v[3].Pos=p->OldPos-Cross;

	v[0].color=D3DCOLOR_ARGB(255,255,255,255);
	v[1].color=D3DCOLOR_ARGB(255,255,255,255);
	v[2].color=D3DCOLOR_ARGB(255,255,255,255);
	v[3].color=D3DCOLOR_ARGB(255,255,255,255);

	v[0].Tex=D3DXVECTOR2(0,0);
	v[1].Tex=D3DXVECTOR2(1,0);
	v[2].Tex=D3DXVECTOR2(1,Len/30.0f);
	v[3].Tex=D3DXVECTOR2(0,Len/30.0f);

	D3DXMATRIX mWorld;

	D3DXMatrixIdentity(&mWorld);
	lpD3DDevice->SetTransform(D3DTS_WORLD, &mWorld);

	lpD3DDevice->SetTexture(0,lpGun);

	lpD3DDevice->DrawPrimitiveUP(D3DPT_TRIANGLEFAN,2,v,sizeof(VERTEX));

	lpD3DDevice->SetRenderState(D3DRS_DESTBLEND,D3DBLEND_INVSRCALPHA);
}


int UpdatePhal(void *p_void,float *Z)
{
	struct PHAL *p=(struct PHAL *)p_void;

	//機銃処理

	if(D3DXVec3Length(&(p->Pos-CameraPos))>3000){
		return 0;
	}

	D3DXMATRIX mTmp;
	D3DXMatrixTranslation(&mTmp,0,0,10+Speed);
	p->Mat=mTmp*p->Mat;
	D3DXVec3TransformCoord(&p->Pos,&D3DXVECTOR3(0,0,0),&p->Mat);

	if(D3DXVec3Length(&(p->OldPos-p->Pos))>=30){
		D3DXVec3TransformCoord(&p->OldPos,&D3DXVECTOR3(0,0,-30),&p->Mat);
	}else{
		D3DXVECTOR3 Vec;
		Vec=p->Pos-p->OldPos;
		D3DXVec3Normalize(&Vec,&Vec);
		p->OldPos+=Vec*Speed;
	}

	//カメラから見たZ位置を算出
	D3DXVECTOR3 TmpVec;
	D3DXVec3TransformCoord(&TmpVec,&((p->Pos+p->OldPos)/2-CameraPos),&mRevCameraRot);
	*Z=TmpVec.z;

	return 1;
}

void PhalInit(D3DXMATRIX Mat)
{
	//機銃登場
	struct PHAL *p;

	p=(struct PHAL *)malloc(sizeof(struct PHAL));

	if(p!=NULL){
		D3DXVECTOR3 TmpPos;
		D3DXVec3TransformCoord(&TmpPos,&D3DXVECTOR3(0,0,0),&Mat);
		p->Mat=Mat;
		p->Flg=1;
		p->Pos=TmpPos;
		p->OldPos=p->Pos;

		ZSortInit(p,RenderPhal,UpdatePhal,p->Pos);
	}
}

int ZComp(const void *a,const void *b)
{
	//正の値を返すと*bが*aの前に並ぶようになる
	
	if(((struct ZSORT *)a)->Z < ((struct ZSORT *)b)->Z){
		return 1;
	}
	if(((struct ZSORT *)a)->Z > ((struct ZSORT *)b)->Z){
		return -1;
	}
	return 0;
}

void LoadMesh(struct XFILE *XFile,char FName[])
{
	LPD3DXBUFFER lpD3DXBuffer;

	D3DXLoadMeshFromX(FName,D3DXMESH_MANAGED,lpD3DDevice,NULL,&lpD3DXBuffer,NULL,&(XFile->NumMaterial),&(XFile->lpMesh));

	XFile->Mat=new D3DMATERIAL9[XFile->NumMaterial];
	XFile->Tex=new LPDIRECT3DTEXTURE9[XFile->NumMaterial];

	D3DXMATERIAL* D3DXMat=(D3DXMATERIAL*)lpD3DXBuffer->GetBufferPointer();

	DWORD i;
	for(i=0;i<XFile->NumMaterial;i++){
		XFile->Mat[i]=D3DXMat[i].MatD3D;
		XFile->Mat[i].Ambient=XFile->Mat[i].Diffuse;
		if(FAILED(D3DXCreateTextureFromFile(lpD3DDevice,D3DXMat[i].pTextureFilename,&(XFile->Tex[i])))){
			XFile->Tex[i]=NULL;
		}
	}

	lpD3DXBuffer->Release();
}

void ReleaseMesh(struct XFILE *XFile)
{
	DWORD i;

	delete[] XFile->Mat;
	for(i=0;i<XFile->NumMaterial;i++){
		if(XFile->Tex[i]!=NULL){
			XFile->Tex[i]->Release();
		}
	}
	delete[] XFile->Tex;
	XFile->lpMesh->Release();
}

void DrawMesh(struct XFILE *XFile)
{
	DWORD i;
	for(i=0;i<XFile->NumMaterial;i++){
		lpD3DDevice->SetMaterial(&(XFile->Mat[i]));
		lpD3DDevice->SetTexture(0,XFile->Tex[i]);
		XFile->lpMesh->DrawSubset(i);
	}
}


void GetCameraAngle3(D3DXVECTOR3 CameraPos,D3DXVECTOR3 LookPos,float *AngX,float *AngY)
{
	//注視点をカメラからの相対座標にする
	LookPos-=CameraPos;

	//Z軸
	D3DXVECTOR3 AxisZ=D3DXVECTOR3(0,0,1);

	//視線のベクトル
	D3DXVECTOR3 TmpVec;

	//内積でX軸回転とY軸回転の角度を算出---------------------------

	//視線ベクトルをXZ面に平面化したベクトル
	TmpVec=D3DXVECTOR3(LookPos.x,0,LookPos.z);
	D3DXVec3Normalize(&TmpVec,&TmpVec);
	D3DXVec3Normalize(&LookPos,&LookPos);

	float Dot;

	//XZ面に平面化したベクトルと視線ベクトルとの内積でX軸回転角度を算出
	Dot=D3DXVec3Dot(&LookPos,&TmpVec);

	//内積が不正な数であれば制限
	if(Dot<-1)Dot=-1;
	if(Dot>1)Dot=1;

	//内積から角度を算出
	*AngX=acosf(Dot);

	//正の値しか出てこないので、向きによって負の値にする
	if(LookPos.y>0)*AngX=-*AngX;

	//XZ面に平面化したベクトルとZ軸との内積でY軸回転角度を算出
	Dot=D3DXVec3Dot(&AxisZ,&TmpVec);

	//内積が不正な数であれば制限
	if(Dot<-1)Dot=-1;
	if(Dot>1)Dot=1;

	//内積から角度を算出
	*AngY=acosf(Dot);

	//正の値しか出てこないので、向きによって負の値にする
	if(LookPos.x<0)*AngY=-*AngY;
}

void GameReset(void)
{

	//ゲームに関連する変数のリセットなど
	int i;

	//自機のヨー・ピッチ・ロール
	Roll=0;
	Pitch=0;
	Yaw=0;

	//機銃の発射間隔
	FireTime=0;

	Speed=3;

	//自機の初期値設定
	D3DXMatrixTranslation(&mPlane,0,20,0);

	//後方視点カメラのための自機姿勢保存配列を初期化
	for(i=0;i<GNUM;i++){
		mGhost[i]=mPlane;
	}
}

void GameEnd(void)
{
	//ゲームリセット時に必要な終了処理

	int i;
	for(i=0;i<ZCnt;i++){
		if(ZSort[i].p!=NULL){
			free(ZSort[i].p);
			ZSort[i].p=NULL;
		}
	}
	ZCnt=0;

}

void RenderProc(void)
{

	int i;

	D3DXMATRIX mTrans,mScale,mRot,mWorld;

	//風景なので、明暗の計算は不要
	lpD3DDevice->SetRenderState(D3DRS_LIGHTING,FALSE);

	//フォグも不要
	lpD3DDevice->SetRenderState(D3DRS_FOGENABLE, FALSE);

	//空表示---------------------------------------------------
	//カメラに合わせて移動
	D3DXMatrixTranslation(&mTrans,CameraPos.x,0,CameraPos.z);
	//大きく拡大して世界を包む
	D3DXMatrixScaling(&mScale,5000,5000,5000);
	mWorld=mScale*mTrans;
	lpD3DDevice->SetTransform(D3DTS_WORLD,&mWorld);
	DrawMesh(&Sky);

	//フォグ計算の設定を戻す
	lpD3DDevice->SetRenderState(D3DRS_FOGENABLE, TRUE);

	//海面---------------------------------------------------
	lpD3DDevice->SetFVF(FVF_VERTEX);
	VERTEX v[4];

	v[0].Pos=D3DXVECTOR3(-10000,0,10000);
	v[1].Pos=D3DXVECTOR3(10000,0,10000);
	v[2].Pos=D3DXVECTOR3(10000,0,-10000);
	v[3].Pos=D3DXVECTOR3(-10000,0,-10000);
	v[0].color=D3DCOLOR_ARGB(255,255,255,255);
	v[1].color=D3DCOLOR_ARGB(255,255,255,255);
	v[2].color=D3DCOLOR_ARGB(255,255,255,255);
	v[3].color=D3DCOLOR_ARGB(255,255,255,255);
	v[0].Tex=D3DXVECTOR2(0.0f,0.0f);
	v[1].Tex=D3DXVECTOR2(10.0f,0.0f);
	v[2].Tex=D3DXVECTOR2(10.0f,10.0f);
	v[3].Tex=D3DXVECTOR2(0.0f,10.0f);
	lpD3DDevice->SetTexture(0,lpSea);

	D3DXMatrixIdentity(&mWorld);
	lpD3DDevice->SetTransform(D3DTS_WORLD, &mWorld);
	lpD3DDevice->DrawPrimitiveUP(D3DPT_TRIANGLEFAN,2,v,sizeof(VERTEX));
	// 海面表示---------------------------------------------------end

	//明暗の計算をするよう、設定を戻す
	lpD3DDevice->SetRenderState(D3DRS_LIGHTING,TRUE);

		//自機表示---------------------------------------------------
		D3DXMATRIX mTmp;
		D3DXMatrixTranslation(&mTrans,(float)(rand()%11-5)/300.0f,(float)(rand()%11-5)/300.0f,0);
		mTmp=mTrans*mPlane;
		lpD3DDevice->SetTransform(D3DTS_WORLD,&mTmp);
		DrawMesh(&Plane);

	//ビルボードの描画----------------------------
	lpD3DDevice->SetRenderState(D3DRS_LIGHTING,FALSE);

	lpD3DDevice->SetFVF(FVF_VERTEX);
	lpD3DDevice->SetRenderState(D3DRS_ZWRITEENABLE,FALSE);

	//裏面カリングを切る
	lpD3DDevice->SetRenderState(D3DRS_CULLMODE,D3DCULL_NONE);

	for(i=0;i<ZCnt;i++){
		ZSort[i].RenderProc(ZSort[i].p);
	}

	//機銃 START
	//機銃発射直後のみ発射光を表示----------------------------------
	if((FireTime>=1)&&(FireTime<=2)){
		//加算合成になるよう設定
		lpD3DDevice->SetRenderState(D3DRS_DESTBLEND,D3DBLEND_ONE);

		v[0].Pos=D3DXVECTOR3(-0.8f,0.8f,0);
		v[1].Pos=D3DXVECTOR3(0.8f,0.8f,0);
		v[2].Pos=D3DXVECTOR3(0.8f,-0.8f,0);
		v[3].Pos=D3DXVECTOR3(-0.8f,-0.8f,0);
		v[0].color=D3DCOLOR_ARGB(255,255,255,255);
		v[1].color=D3DCOLOR_ARGB(255,255,255,255);
		v[2].color=D3DCOLOR_ARGB(255,255,255,255);
		v[3].color=D3DCOLOR_ARGB(255,255,255,255);
		v[0].Tex=D3DXVECTOR2(0.0f,0.0f);
		v[1].Tex=D3DXVECTOR2(1.0f,0.0f);
		v[2].Tex=D3DXVECTOR2(1.0f,1.0f);
		v[3].Tex=D3DXVECTOR2(0.0f,1.0f);
		lpD3DDevice->SetTexture(0,lpFlash);

		D3DXMATRIX mOffset;

		//自機に対して左右に表示
		for(i=-1;i<=1;i+=2){
			D3DXMatrixTranslation(&mOffset,1.7f*i,0,1.0f);
			mWorld=mOffset*mPlane;
			lpD3DDevice->SetTransform(D3DTS_WORLD, &mWorld);
			lpD3DDevice->DrawPrimitiveUP(D3DPT_TRIANGLEFAN,2,v,sizeof(VERTEX));
		}

		lpD3DDevice->SetRenderState(D3DRS_DESTBLEND,D3DBLEND_INVSRCALPHA);
	}
	//機銃 END

	lpD3DDevice->SetRenderState(D3DRS_ZWRITEENABLE,TRUE);

	//裏面カリングの設定を戻す
	lpD3DDevice->SetRenderState(D3DRS_CULLMODE,D3DCULL_CCW);
}


void UpdateProc(void)
{

	int i,j;

	D3DXMATRIX mRot;
	D3DXMATRIX mTrans;

		//機銃 START
		//機銃発射
		if(GetAsyncKeyState(VK_SPACE)&0x8000){
			if(SpaceKeyFlg!=2){
				SpaceKeyFlg=1;

				//一定間隔で機銃を発射する（毎フレーム発射しない）
				if(FireTime==0){
					D3DXMATRIX mTmp;

					//自機の左右から発射する
					for(j=-1;j<=1;j+=2){
						D3DXMatrixTranslation(&mTmp,1.9f*j,0,-1.5f);
						mTmp=mTmp*mPlane;
						PhalInit(mTmp);
					}
				}

				FireTime++;
				if(FireTime>6){
					FireTime=0;
				}
			}
		}
		if(!(GetAsyncKeyState(VK_SPACE)&0x8000)){
			FireTime=0;
			if(SpaceKeyFlg==2)SpaceKeyFlg=0;
		}
		//機銃 END

		//ヨーの操作
		if((GetAsyncKeyState(VK_LEFT)&0x8000)||(GetAsyncKeyState('A')&0x8000)){
			Yaw-=0.02f;
			if(Yaw<-0.5f)Yaw=-0.5f;
		}
		if((GetAsyncKeyState(VK_RIGHT)&0x8000)||(GetAsyncKeyState('D')&0x8000)){
			Yaw+=0.02f;
			if(Yaw>0.5f)Yaw=0.5f;
		}

		//ピッチの操作
		if((GetAsyncKeyState(VK_UP)&0x8000)||(GetAsyncKeyState('W')&0x8000)){
			Pitch+=0.05f;
			if(Pitch>2)Pitch=2;
		}
		if((GetAsyncKeyState(VK_DOWN)&0x8000)||(GetAsyncKeyState('S')&0x8000)){
			Pitch-=0.05f;
			if(Pitch<-2)Pitch=-2;
		}

		//ロールの操作
		if(GetAsyncKeyState('Q')&0x8000){
			Roll+=0.05f;
			if(Roll>2)Roll=2;
		}
		if(GetAsyncKeyState('E')&0x8000){
			Roll-=0.05f;
			if(Roll<-2)Roll=-2;
		}

		//現在のロールの勢いに合わせて回転
		D3DXMatrixRotationZ(&mRot,D3DXToRadian(Roll));
		mPlane=mRot*mPlane;

		//現在のヨーの勢いに合わせて回転
		D3DXMatrixRotationY(&mRot,D3DXToRadian(Yaw));
		mPlane=mRot*mPlane;

		//現在のピッチの勢いに合わせて回転
		D3DXMatrixRotationX(&mRot,D3DXToRadian(Pitch));
		mPlane=mRot*mPlane;


		//噴射 START
		if(GetAsyncKeyState('Z')&0x8000){
			if(KeyFlg!=2){
				KeyFlg=1;


				Speed++;
				if(Speed>18){
					Speed=18;
				}
			}
		}

		if(!(GetAsyncKeyState('Z')&0x8000)){

			if(KeyFlg==2) {
				KeyFlg=0;
			}
		}

		//噴射 END


		//現在の速度に合わせて自機を前進
		D3DXMatrixTranslation(&mTrans,0,0,Speed);
		mPlane=mTrans*mPlane;

		//墜落したかどうか
		if(mPlane._42<0){

			//(水面の下へもぐりこませない)
			mPlane._42=2;
			
			Pitch-=0.25f;
			if(Pitch<-0.25f)Pitch=-0.25f;

		}

		//(範囲無限に変更)
		if(mPlane._41<-5000){
			mPlane._41=0;
		}
		if(mPlane._41>5000){
			mPlane._41=0;
		}
		if(mPlane._43<-5000){
			mPlane._43=0;
		}
		if(mPlane._43>5000){
			mPlane._43=0;
		}

		/*
		//10000四方が移動範囲(デフォルト)
		if(mPlane._41<-5000){
			mPlane._41=-5000;
		}
		if(mPlane._41>5000){
			mPlane._41=5000;
		}
		if(mPlane._43<-5000){
			mPlane._43=-5000;
		}
		if(mPlane._43>5000){
			mPlane._43=5000;
		}
		*/


	//ビルボードのUpdateProcを実行
	for(i=0;i<ZCnt;i++){
		if(!ZSort[i].UpdateProc(ZSort[i].p,&(ZSort[i].Z))){
			//Zソートセルが不要になったら

			//今のセルのパラメータメモリを解放
			free(ZSort[i].p);

			//個数をひとつ減らす
			ZCnt--;

			//今のセルが末尾でないなら末尾セルを今のセルに上書きする
			if(i!=ZCnt){
				ZSort[i]=ZSort[ZCnt];

				//i番目にコピーされたビルボードは処理が終わっていないので
				//iを減らして再実行させる
				i--;
				continue;
			}
		}
	}

	//ビルボード用構造体のソート
	qsort(ZSort,ZCnt,sizeof(struct ZSORT),ZComp);

	//後方視点カメラのために自機姿勢を保存
	for(i=GNUM-1;i>0;i--){
		mGhost[i]=mGhost[i-1];
		mGhost[i]._41=mPlane._41;
		mGhost[i]._42=mPlane._42;
		mGhost[i]._43=mPlane._43;
	}
	mGhost[0]=mPlane;

}

void DrawFrame(void)
{

	//秒間６０フレームを越えないようにする
	static DWORD NowTime,PrevTime;
	NowTime = timeGetTime();
	if( (NowTime - PrevTime)<1000/60 )return;
	PrevTime = NowTime;

	//処理系関数
	UpdateProc();

	//自機の座標を割り出す
	GhostPos=D3DXVECTOR3(0,1.7f,0);
	HeadVec=D3DXVECTOR3(0,1,0)+GhostPos;
	D3DXVec3TransformCoord(&GhostPos,&GhostPos,&mGhost[GNUM-1]);

	CameraPos=D3DXVECTOR3(0,2,-8);	//カメラは自機よりも10後方にいる
	D3DXVec3TransformCoord(&CameraPos,&CameraPos,&mGhost[GNUM-1]);

	D3DXVec3TransformCoord(&HeadVec,&HeadVec,&mGhost[GNUM-1]);
	HeadVec-=GhostPos;

	float AngX,AngY;
	GetCameraAngle3(CameraPos,GhostPos,&AngX,&AngY);
	RotX=D3DXToDegree(AngX);
	RotY=D3DXToDegree(AngY);

	//ビルボード用にカメラの逆回転の行列を作成
	D3DXMATRIX mCameraRotX,mCameraRotY;
	D3DXMatrixRotationX(&mCameraRotX,D3DXToRadian(-RotX));
	D3DXMatrixRotationY(&mCameraRotY,D3DXToRadian(-RotY));
	mRevCameraRot=mCameraRotY*mCameraRotX;

//------------------------------------------------------------------

	// 描画開始
	lpD3DDevice->BeginScene();

	D3DXMATRIX mView,mProj;

//------------------------------------------------------------------


	// バックバッファと Z バッファをクリア
	lpD3DDevice->Clear(0,NULL,D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER,D3DCOLOR_XRGB(0,0,255),1.0f,0);

	// 視点行列の設定
	D3DXMatrixLookAtLH(&mView,
		&CameraPos,
		&GhostPos,
		&HeadVec
	);

	// 投影行列の設定
	D3DXMatrixPerspectiveFovLH(&mProj, D3DXToRadian(45), 4.0f/3.0f, 1.0f, 10000.0f);

	//行列設定
	lpD3DDevice->SetTransform(D3DTS_VIEW, &mView);
	lpD3DDevice->SetTransform(D3DTS_PROJECTION, &mProj);

	//描画系関数
	RenderProc();

	//スプライト関連の表示処理
	// lpSprite->Begin(D3DXSPRITE_ALPHABLEND);
	D3DXMATRIX mWorld;
	// TIME 表示
	//RECT rcTime={80,73,135,100};
	//lpSprite->Draw(lpNumber,&rcTime,NULL,&D3DXVECTOR3(220,10,0),D3DCOLOR_ARGB(255,255,255,255));
	// lpSprite->End();

	//------------------------------------------------------------------
	//デバッグ用変数
	char chscr1[10];
	char chscr2[10];
	char chscr3[10];
	char chscr4[10];
	char chscr5[10];
	char chscr6[10];

	RECT Font1rc={20,20,640,320};
	RECT Font2rc={20,40,640,320};
	RECT Font3rc={20,60,640,320};
	RECT Font4rc={20,80,640,320};
	RECT Font5rc={20,100,640,320};
	RECT Font6rc={20,120,640,320};

	//VC++2003 まで
	sprintf(chscr1,"%09ld",CameraPos);
	sprintf(chscr2,"%09ld",GhostPos);
	sprintf(chscr3,"%09ld",HeadVec);
	sprintf(chscr4,"%09ld",mPlane);
	sprintf(chscr5,"%09ld",mWorld);
	sprintf(chscr6,"%09ld",Speed);
	//sprintf(chscr7,"%09ld",Num);

	//VC++2005以降
	// sprintf_s(chscr1,sizeof(chscr),"%09ld",CameraPos);
	// sprintf_s(chscr2,sizeof(chscr),"%09ld",GhostPos);
	// sprintf_s(chscr3,sizeof(chscr),"%09ld",HeadVec);
	// sprintf_s(chscr4,sizeof(chscr),"%09ld",mPlane);
	// sprintf_s(chscr5,sizeof(chscr),"%09ld",mWorld);
	// sprintf_s(chscr6,sizeof(chscr),"%09ld",Num);

	lpFont->DrawText( NULL , chscr1 , -1 , &Font1rc , NULL , 0xFF88FF88 );
	lpFont->DrawText( NULL , chscr2 , -1 , &Font2rc , NULL , 0xFF88FF88 );
	lpFont->DrawText( NULL , chscr3 , -1 , &Font3rc , NULL , 0xFF88FF88 );
	lpFont->DrawText( NULL , chscr4 , -1 , &Font4rc , NULL , 0xFF88FF88 );
	lpFont->DrawText( NULL , chscr5 , -1 , &Font5rc , NULL , 0xFF88FF88 );
	lpFont->DrawText( NULL , chscr6 , -1 , &Font6rc , NULL , 0xFF88FF88 );
	//------------------------------------------------------------------

	// 描画終了
	lpD3DDevice->EndScene();

	// バックバッファをプライマリバッファにコピー
	lpD3DDevice->Present(NULL,NULL,NULL,NULL);
}

LRESULT APIENTRY WndFunc(HWND hwnd,UINT msg,WPARAM wParam,LPARAM lParam)
{
	switch(msg){
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	case WM_KEYDOWN:
		switch(wParam){
		case VK_ESCAPE:
			PostQuitMessage(0);
			return 0;
		}
		return 0;

	}
	return DefWindowProc(hwnd,msg,wParam,lParam);

}

void LoadText(LPDIRECT3DTEXTURE9 *lpTex,char fname[],int W,int H,D3DCOLOR Color)
{
	if(W==0)W=D3DX_DEFAULT;
	if(H==0)H=D3DX_DEFAULT;
	D3DXCreateTextureFromFileEx(lpD3DDevice,fname,W,H,1,0,D3DFMT_UNKNOWN,D3DPOOL_MANAGED,D3DX_FILTER_NONE,D3DX_DEFAULT,Color,NULL,NULL,lpTex);
}

int WINAPI WinMain(HINSTANCE hInst,HINSTANCE hPrev,
				   LPSTR lpszCmdParam,int nCmdshow)
{
	MSG msg;

	HWND hwnd;
	WNDCLASS wc;
	char szAppName[]="Generic Game SDK Window";

	wc.style=CS_DBLCLKS;
	wc.lpfnWndProc=WndFunc;
	wc.cbClsExtra=0;
	wc.cbWndExtra=0;
	wc.hInstance=hInst;
	wc.hIcon=LoadIcon(NULL,IDI_APPLICATION);
	wc.hCursor=LoadCursor(NULL,IDC_ARROW);
	wc.hbrBackground=(HBRUSH)GetStockObject(WHITE_BRUSH);
	wc.lpszMenuName=NULL;
	wc.lpszClassName=szAppName;

	RegisterClass(&wc);

	hwnd=CreateWindowEx(
		0,
		szAppName,
		"Direct X",
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		SCRW,SCRH,
		NULL,NULL,hInst,
		NULL);

	if(!hwnd)return FALSE;

	ShowWindow(hwnd,nCmdshow);
	UpdateWindow(hwnd);
	SetFocus(hwnd);
	if(FULLSCREEN){
		ShowCursor(FALSE);
	}else{
		RECT rc={0,0,SCRW,SCRH};
		AdjustWindowRect(&rc,WS_OVERLAPPEDWINDOW,FALSE);
		SetWindowPos(hwnd,NULL,0,0,rc.right-rc.left,rc.bottom-rc.top,SWP_SHOWWINDOW|SWP_NOMOVE|SWP_NOZORDER);
	}

	//---------------------DirectX Graphics関連-----------------------

	// Direct3D オブジェクトを作成
	lpD3D = Direct3DCreate9(D3D_SDK_VERSION);
	if (!lpD3D)
	{
		// オブジェクト作成失敗
		MessageBox(NULL,"Direct3D の作成に失敗しました。","ERROR",MB_OK | MB_ICONSTOP);
		// 終了する
		return 0;
	}
	int adapter;

	// 使用するアダプタ番号
	adapter = D3DADAPTER_DEFAULT;

	// ウインドウの作成が完了したので、Direct3D を初期化する
	ZeroMemory(&d3dpp,sizeof(D3DPRESENT_PARAMETERS));
	// Direct3D 初期化パラメータの設定
	if (FULLSCREEN)
	{
		// フルスクリーン : ほとんどのアダプタでサポートされているフォーマットを使用
		d3dpp.BackBufferFormat = D3DFMT_X8R8G8B8;
	}
	else
	{
		// ウインドウ : 現在の画面モードを使用
		D3DDISPLAYMODE disp;
		// 現在の画面モードを取得
		lpD3D->GetAdapterDisplayMode(adapter,&disp);
		d3dpp.BackBufferFormat = disp.Format;
	}
	// 表示領域サイズの設定
	d3dpp.BackBufferWidth = SCRW;
	d3dpp.BackBufferHeight = SCRH;
	d3dpp.SwapEffect = D3DSWAPEFFECT_FLIP;

	if (!FULLSCREEN)
	{
		// ウインドウモード
		d3dpp.Windowed = 1;
	}

	// Z バッファの自動作成
	d3dpp.EnableAutoDepthStencil = 1;
	d3dpp.AutoDepthStencilFormat = D3DFMT_D16;

	//ﾊﾞｯｸﾊﾞｯﾌｧをﾛｯｸ可能にする(GetDCも可能になる)
	d3dpp.Flags=D3DPRESENTFLAG_LOCKABLE_BACKBUFFER;

	// デバイスの作成 - T&L HAL
	if (FAILED(lpD3D->CreateDevice(adapter,D3DDEVTYPE_HAL,hwnd,D3DCREATE_HARDWARE_VERTEXPROCESSING,&d3dpp,&lpD3DDevice)))
	{
		// 失敗したので HAL で試行
		if (FAILED(lpD3D->CreateDevice(adapter,D3DDEVTYPE_HAL,hwnd,D3DCREATE_SOFTWARE_VERTEXPROCESSING,&d3dpp,&lpD3DDevice)))
		{
			// 失敗したので REF で試行
			if (FAILED(lpD3D->CreateDevice(adapter,D3DDEVTYPE_REF,hwnd,D3DCREATE_SOFTWARE_VERTEXPROCESSING,&d3dpp,&lpD3DDevice)))
			{
				// 結局失敗した
				MessageBox(NULL,"DirectX9が初期化できません。\n未対応のパソコンと思われます。","ERROR",MB_OK | MB_ICONSTOP);
				lpD3D->Release();
				// 終了する
				return 0;
			}
		}
	}

	// レンダリング・ステートを設定
	// Z バッファ有効化->前後関係の計算を正確にしてくれる
	lpD3DDevice->SetRenderState(D3DRS_ZENABLE,D3DZB_TRUE);
	lpD3DDevice->SetRenderState(D3DRS_ZWRITEENABLE,TRUE);

	// アルファブレンディング有効化
	lpD3DDevice->SetRenderState(D3DRS_ALPHABLENDENABLE,TRUE);

	// アルファブレンディング方法を設定
	lpD3DDevice->SetRenderState(D3DRS_SRCBLEND,D3DBLEND_SRCALPHA);
	lpD3DDevice->SetRenderState(D3DRS_DESTBLEND,D3DBLEND_INVSRCALPHA);

	// レンダリング時のアルファ値の計算方法の設定
	lpD3DDevice->SetTextureStageState(0,D3DTSS_ALPHAOP,D3DTOP_MODULATE);
	// テクスチャの色を使用
	lpD3DDevice->SetTextureStageState(0,D3DTSS_ALPHAARG1,D3DTA_TEXTURE);
	// 頂点の色を使用
	lpD3DDevice->SetTextureStageState(0,D3DTSS_ALPHAARG2,D3DTA_DIFFUSE);
	// レンダリング時の色の計算方法の設定
	lpD3DDevice->SetTextureStageState(0,D3DTSS_COLOROP,D3DTOP_MODULATE);

	//裏面カリング
	lpD3DDevice->SetRenderState(D3DRS_CULLMODE,D3DCULL_NONE);

	// フィルタ設定
	lpD3DDevice->SetSamplerState(0,D3DSAMP_MIPFILTER,D3DTEXF_LINEAR);
	lpD3DDevice->SetSamplerState(0,D3DSAMP_MINFILTER,D3DTEXF_LINEAR);
	lpD3DDevice->SetSamplerState(0,D3DSAMP_MAGFILTER,D3DTEXF_LINEAR);

	//ライトの設置
	D3DLIGHT9 Light;

    ZeroMemory(&Light, sizeof(D3DLIGHT9));
    Light.Type       = D3DLIGHT_DIRECTIONAL;             // 光源の種類
    Light.Diffuse.r  = 1.0f;                             // ディフューズ Ｒ
    Light.Diffuse.g  = 1.0f;                             // ディフューズ Ｇ
    Light.Diffuse.b  = 1.0f;                             // ディフューズ Ｂ
    Light.Diffuse.a  = 0.0f;                             // ディフューズ Ａ
    Light.Direction  = D3DXVECTOR3(1.0f,-1.0f,1.0f);     // 光の進む方向

    lpD3DDevice->SetLight(0, &Light);
    lpD3DDevice->LightEnable(0, TRUE);


	//メッシュ（Ｘファイル）の読み込み
	LoadMesh(&Plane,"data\\AirPlane.x");	//自機
	LoadMesh(&Sky,"data\\Sky.x");			//空
	
	//テクスチャの読み込み
	LoadText(&lpSea,"data\\sea.bmp",256,256,0);		//海
	LoadText(&lpFlash,"data\\Flash.bmp",24,24,0);	//発射光(機銃)
	LoadText(&lpGun,"data\\EPhal.bmp",16,16,D3DCOLOR_XRGB(0,0,0));	//機銃


	//文字追加
	D3DXCreateFont( lpD3DDevice , 20 , 10 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , "Times New Roman" , &lpFont );
	
	//フォグの設定
	float Start = 3000.0f;
	float End   = 4000.0f;
	lpD3DDevice->SetRenderState(D3DRS_FOGTABLEMODE, D3DFOG_LINEAR);
	lpD3DDevice->SetRenderState(D3DRS_FOGSTART, *(DWORD *)(&Start));
	lpD3DDevice->SetRenderState(D3DRS_FOGEND,   *(DWORD *)(&End));
	lpD3DDevice->SetRenderState(D3DRS_FOGENABLE, TRUE);
	lpD3DDevice->SetRenderState(D3DRS_FOGCOLOR,D3DCOLOR_XRGB(200,200,200));

	srand(timeGetTime()%32767);

	//ゲームに関連する変数のリセット
	GameReset();

	timeBeginPeriod(1);

	while(1){
		if(PeekMessage(&msg,NULL,0,0,PM_NOREMOVE))
		{
			if(!GetMessage(&msg,NULL,0,0))
				break;
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}else{
			DrawFrame();
		}
	}

	//ゲームに関する終了処理

	timeEndPeriod(1);

	//リセットごとに必要なゲームの終了処理
	GameEnd();

	//メッシュを解放
	ReleaseMesh(&Plane);
	ReleaseMesh(&Sky);


	//テクスチャを解放
	lpSea->Release();
	lpFlash->Release();
	lpGun->Release();


	//文字を解放
	lpFont->Release();

	// Direct3D オブジェクトを解放
	lpD3DDevice->Release();
	lpD3D->Release();

	return msg.wParam;
}

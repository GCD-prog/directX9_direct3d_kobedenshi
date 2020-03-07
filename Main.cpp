#include<windows.h>
#include<stdio.h>
#include<d3dx9.h>
#include<time.h>

#pragma comment(lib, "d3d9.lib")
#pragma comment(lib, "d3dx9.lib")
#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "winmm.lib")

#define	FULLSCREEN	0		// �t���X�N���[������ 1 �ɂ���
#define	SCRW		640		// Width
#define	SCRH		480		// Height

LPDIRECT3D9				lpD3D;		// Direct3D�C���^�[�t�F�C�X
LPDIRECT3DDEVICE9		lpD3DDevice;	// Direct3DDevice�C���^�[�t�F�C�X
D3DPRESENT_PARAMETERS d3dpp;

#define ZNUM 2000	//�r���{�[�h�̐�

#define GNUM 20		//������_�̃J�����̒x��
//#define SPEED 3		//���@�̑��x

LPD3DXFONT lpFont = NULL; //�����t�H���g

LPDIRECT3DTEXTURE9 lpSea;	//�C�ʃe�N�X�`��
LPDIRECT3DTEXTURE9 lpFlash;	//���ˌ��e�N�X�`��
LPDIRECT3DTEXTURE9 lpGun;	//�@�e�e�N�X�`��


//���O��Z��Ă��邽�߂̍\����
struct ZSORT{
	void *p;	//���̃r���{�[�h�ɕK�v�ȍ\���̂ւ̃|�C���^(NULL:���g�p)
	float Z;	//Z���W
	void (*RenderProc)(void *);
	int (*UpdateProc)(void *,float *);
};

struct ZSORT ZSort[ZNUM];
int ZCnt;

// ����g�p���钸�_�t�H�[�}�b�g�̒�`
struct VERTEX
{
	D3DXVECTOR3 Pos;
	D3DCOLOR color;
	D3DXVECTOR2 Tex;
};

//�@�e�p �\����
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

//���b�V���Ǘ��p �\����
struct XFILE{
	LPD3DXMESH lpMesh;
	DWORD NumMaterial;
	D3DMATERIAL9 *Mat;
	LPDIRECT3DTEXTURE9 *Tex;
};

//�e�탁�b�V���p
struct XFILE Plane;	//���@
struct XFILE Sky;	//��

//���@�̃��[�E�s�b�`�E���[���̐���
float Roll,Pitch,Yaw;

//�J�����̌����i�r���{�[�h�̂��߂Ɏg�p�j
float RotX,RotY;

//�J�����֘A
D3DXVECTOR3 GhostPos;	//�J�����̒������W
D3DXVECTOR3 CameraPos;	//�J�����̍��W
D3DXVECTOR3 HeadVec;	//�J�����̓������

//���@�̏�Ԃ�\���s��
D3DXMATRIX mPlane;

//���@�̏�Ԃ̗��� �i�[�p
D3DXMATRIX mGhost[GNUM];

//���݂̃J�����̋t�s��
D3DXMATRIX mRevCameraRot;

//�@�e�̔��ˊԊu
int FireTime;

float Speed;

//�X�y�[�X�L�[�A�˖h�~�t���O
int SpaceKeyFlg;
int KeyFlg;

void ZSortInit(void *p,void (*ppRenderProc)(void *),int (*ppUpdateProc)(void *,float *),D3DXVECTOR3 Pos)
{
	//��x����UpdateProc�����s���A�����ʒu�ɂ�����J����Z�ʒu���Z�o���Ă���
	float Z;
	ppUpdateProc(p,&Z);

	if(ZCnt<ZNUM){
		ZSort[ZCnt].p=p;
		ZSort[ZCnt].RenderProc=ppRenderProc;
		ZSort[ZCnt].UpdateProc=ppUpdateProc;

		ZSort[ZCnt].Z=Z;

		ZCnt++;
	}else{
		//�r���{�[�h������Ȃ����
		//�����Ƃ��������̂��폜
		free(ZSort[0].p);

		//�����������ɐV�o��
		ZSort[0].p=p;
		ZSort[0].RenderProc=ppRenderProc;
		ZSort[0].UpdateProc=ppUpdateProc;

		ZSort[0].Z=Z;
	}
}

// �@�e�����_�����O
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

	//�@�e����

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

	//�J�������猩��Z�ʒu���Z�o
	D3DXVECTOR3 TmpVec;
	D3DXVec3TransformCoord(&TmpVec,&((p->Pos+p->OldPos)/2-CameraPos),&mRevCameraRot);
	*Z=TmpVec.z;

	return 1;
}

void PhalInit(D3DXMATRIX Mat)
{
	//�@�e�o��
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
	//���̒l��Ԃ���*b��*a�̑O�ɕ��Ԃ悤�ɂȂ�
	
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
	//�����_���J��������̑��΍��W�ɂ���
	LookPos-=CameraPos;

	//Z��
	D3DXVECTOR3 AxisZ=D3DXVECTOR3(0,0,1);

	//�����̃x�N�g��
	D3DXVECTOR3 TmpVec;

	//���ς�X����]��Y����]�̊p�x���Z�o---------------------------

	//�����x�N�g����XZ�ʂɕ��ʉ������x�N�g��
	TmpVec=D3DXVECTOR3(LookPos.x,0,LookPos.z);
	D3DXVec3Normalize(&TmpVec,&TmpVec);
	D3DXVec3Normalize(&LookPos,&LookPos);

	float Dot;

	//XZ�ʂɕ��ʉ������x�N�g���Ǝ����x�N�g���Ƃ̓��ς�X����]�p�x���Z�o
	Dot=D3DXVec3Dot(&LookPos,&TmpVec);

	//���ς��s���Ȑ��ł���ΐ���
	if(Dot<-1)Dot=-1;
	if(Dot>1)Dot=1;

	//���ς���p�x���Z�o
	*AngX=acosf(Dot);

	//���̒l�����o�Ă��Ȃ��̂ŁA�����ɂ���ĕ��̒l�ɂ���
	if(LookPos.y>0)*AngX=-*AngX;

	//XZ�ʂɕ��ʉ������x�N�g����Z���Ƃ̓��ς�Y����]�p�x���Z�o
	Dot=D3DXVec3Dot(&AxisZ,&TmpVec);

	//���ς��s���Ȑ��ł���ΐ���
	if(Dot<-1)Dot=-1;
	if(Dot>1)Dot=1;

	//���ς���p�x���Z�o
	*AngY=acosf(Dot);

	//���̒l�����o�Ă��Ȃ��̂ŁA�����ɂ���ĕ��̒l�ɂ���
	if(LookPos.x<0)*AngY=-*AngY;
}

void GameReset(void)
{

	//�Q�[���Ɋ֘A����ϐ��̃��Z�b�g�Ȃ�
	int i;

	//���@�̃��[�E�s�b�`�E���[��
	Roll=0;
	Pitch=0;
	Yaw=0;

	//�@�e�̔��ˊԊu
	FireTime=0;

	Speed=3;

	//���@�̏����l�ݒ�
	D3DXMatrixTranslation(&mPlane,0,20,0);

	//������_�J�����̂��߂̎��@�p���ۑ��z���������
	for(i=0;i<GNUM;i++){
		mGhost[i]=mPlane;
	}
}

void GameEnd(void)
{
	//�Q�[�����Z�b�g���ɕK�v�ȏI������

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

	//���i�Ȃ̂ŁA���Â̌v�Z�͕s�v
	lpD3DDevice->SetRenderState(D3DRS_LIGHTING,FALSE);

	//�t�H�O���s�v
	lpD3DDevice->SetRenderState(D3DRS_FOGENABLE, FALSE);

	//��\��---------------------------------------------------
	//�J�����ɍ��킹�Ĉړ�
	D3DXMatrixTranslation(&mTrans,CameraPos.x,0,CameraPos.z);
	//�傫���g�債�Đ��E����
	D3DXMatrixScaling(&mScale,5000,5000,5000);
	mWorld=mScale*mTrans;
	lpD3DDevice->SetTransform(D3DTS_WORLD,&mWorld);
	DrawMesh(&Sky);

	//�t�H�O�v�Z�̐ݒ��߂�
	lpD3DDevice->SetRenderState(D3DRS_FOGENABLE, TRUE);

	//�C��---------------------------------------------------
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
	// �C�ʕ\��---------------------------------------------------end

	//���Â̌v�Z������悤�A�ݒ��߂�
	lpD3DDevice->SetRenderState(D3DRS_LIGHTING,TRUE);

		//���@�\��---------------------------------------------------
		D3DXMATRIX mTmp;
		D3DXMatrixTranslation(&mTrans,(float)(rand()%11-5)/300.0f,(float)(rand()%11-5)/300.0f,0);
		mTmp=mTrans*mPlane;
		lpD3DDevice->SetTransform(D3DTS_WORLD,&mTmp);
		DrawMesh(&Plane);

	//�r���{�[�h�̕`��----------------------------
	lpD3DDevice->SetRenderState(D3DRS_LIGHTING,FALSE);

	lpD3DDevice->SetFVF(FVF_VERTEX);
	lpD3DDevice->SetRenderState(D3DRS_ZWRITEENABLE,FALSE);

	//���ʃJ�����O��؂�
	lpD3DDevice->SetRenderState(D3DRS_CULLMODE,D3DCULL_NONE);

	for(i=0;i<ZCnt;i++){
		ZSort[i].RenderProc(ZSort[i].p);
	}

	//�@�e START
	//�@�e���˒���̂ݔ��ˌ���\��----------------------------------
	if((FireTime>=1)&&(FireTime<=2)){
		//���Z�����ɂȂ�悤�ݒ�
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

		//���@�ɑ΂��č��E�ɕ\��
		for(i=-1;i<=1;i+=2){
			D3DXMatrixTranslation(&mOffset,1.7f*i,0,1.0f);
			mWorld=mOffset*mPlane;
			lpD3DDevice->SetTransform(D3DTS_WORLD, &mWorld);
			lpD3DDevice->DrawPrimitiveUP(D3DPT_TRIANGLEFAN,2,v,sizeof(VERTEX));
		}

		lpD3DDevice->SetRenderState(D3DRS_DESTBLEND,D3DBLEND_INVSRCALPHA);
	}
	//�@�e END

	lpD3DDevice->SetRenderState(D3DRS_ZWRITEENABLE,TRUE);

	//���ʃJ�����O�̐ݒ��߂�
	lpD3DDevice->SetRenderState(D3DRS_CULLMODE,D3DCULL_CCW);
}


void UpdateProc(void)
{

	int i,j;

	D3DXMATRIX mRot;
	D3DXMATRIX mTrans;

		//�@�e START
		//�@�e����
		if(GetAsyncKeyState(VK_SPACE)&0x8000){
			if(SpaceKeyFlg!=2){
				SpaceKeyFlg=1;

				//���Ԋu�ŋ@�e�𔭎˂���i���t���[�����˂��Ȃ��j
				if(FireTime==0){
					D3DXMATRIX mTmp;

					//���@�̍��E���甭�˂���
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
		//�@�e END

		//���[�̑���
		if((GetAsyncKeyState(VK_LEFT)&0x8000)||(GetAsyncKeyState('A')&0x8000)){
			Yaw-=0.02f;
			if(Yaw<-0.5f)Yaw=-0.5f;
		}
		if((GetAsyncKeyState(VK_RIGHT)&0x8000)||(GetAsyncKeyState('D')&0x8000)){
			Yaw+=0.02f;
			if(Yaw>0.5f)Yaw=0.5f;
		}

		//�s�b�`�̑���
		if((GetAsyncKeyState(VK_UP)&0x8000)||(GetAsyncKeyState('W')&0x8000)){
			Pitch+=0.05f;
			if(Pitch>2)Pitch=2;
		}
		if((GetAsyncKeyState(VK_DOWN)&0x8000)||(GetAsyncKeyState('S')&0x8000)){
			Pitch-=0.05f;
			if(Pitch<-2)Pitch=-2;
		}

		//���[���̑���
		if(GetAsyncKeyState('Q')&0x8000){
			Roll+=0.05f;
			if(Roll>2)Roll=2;
		}
		if(GetAsyncKeyState('E')&0x8000){
			Roll-=0.05f;
			if(Roll<-2)Roll=-2;
		}

		//���݂̃��[���̐����ɍ��킹�ĉ�]
		D3DXMatrixRotationZ(&mRot,D3DXToRadian(Roll));
		mPlane=mRot*mPlane;

		//���݂̃��[�̐����ɍ��킹�ĉ�]
		D3DXMatrixRotationY(&mRot,D3DXToRadian(Yaw));
		mPlane=mRot*mPlane;

		//���݂̃s�b�`�̐����ɍ��킹�ĉ�]
		D3DXMatrixRotationX(&mRot,D3DXToRadian(Pitch));
		mPlane=mRot*mPlane;


		//���� START
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

		//���� END


		//���݂̑��x�ɍ��킹�Ď��@��O�i
		D3DXMatrixTranslation(&mTrans,0,0,Speed);
		mPlane=mTrans*mPlane;

		//�ė��������ǂ���
		if(mPlane._42<0){

			//(���ʂ̉��ւ����肱�܂��Ȃ�)
			mPlane._42=2;
			
			Pitch-=0.25f;
			if(Pitch<-0.25f)Pitch=-0.25f;

		}

		//(�͈͖����ɕύX)
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
		//10000�l�����ړ��͈�(�f�t�H���g)
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


	//�r���{�[�h��UpdateProc�����s
	for(i=0;i<ZCnt;i++){
		if(!ZSort[i].UpdateProc(ZSort[i].p,&(ZSort[i].Z))){
			//Z�\�[�g�Z�����s�v�ɂȂ�����

			//���̃Z���̃p�����[�^�����������
			free(ZSort[i].p);

			//�����ЂƂ��炷
			ZCnt--;

			//���̃Z���������łȂ��Ȃ疖���Z�������̃Z���ɏ㏑������
			if(i!=ZCnt){
				ZSort[i]=ZSort[ZCnt];

				//i�ԖڂɃR�s�[���ꂽ�r���{�[�h�͏������I����Ă��Ȃ��̂�
				//i�����炵�čĎ��s������
				i--;
				continue;
			}
		}
	}

	//�r���{�[�h�p�\���̂̃\�[�g
	qsort(ZSort,ZCnt,sizeof(struct ZSORT),ZComp);

	//������_�J�����̂��߂Ɏ��@�p����ۑ�
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

	//�b�ԂU�O�t���[�����z���Ȃ��悤�ɂ���
	static DWORD NowTime,PrevTime;
	NowTime = timeGetTime();
	if( (NowTime - PrevTime)<1000/60 )return;
	PrevTime = NowTime;

	//�����n�֐�
	UpdateProc();

	//���@�̍��W������o��
	GhostPos=D3DXVECTOR3(0,1.7f,0);
	HeadVec=D3DXVECTOR3(0,1,0)+GhostPos;
	D3DXVec3TransformCoord(&GhostPos,&GhostPos,&mGhost[GNUM-1]);

	CameraPos=D3DXVECTOR3(0,2,-8);	//�J�����͎��@����10����ɂ���
	D3DXVec3TransformCoord(&CameraPos,&CameraPos,&mGhost[GNUM-1]);

	D3DXVec3TransformCoord(&HeadVec,&HeadVec,&mGhost[GNUM-1]);
	HeadVec-=GhostPos;

	float AngX,AngY;
	GetCameraAngle3(CameraPos,GhostPos,&AngX,&AngY);
	RotX=D3DXToDegree(AngX);
	RotY=D3DXToDegree(AngY);

	//�r���{�[�h�p�ɃJ�����̋t��]�̍s����쐬
	D3DXMATRIX mCameraRotX,mCameraRotY;
	D3DXMatrixRotationX(&mCameraRotX,D3DXToRadian(-RotX));
	D3DXMatrixRotationY(&mCameraRotY,D3DXToRadian(-RotY));
	mRevCameraRot=mCameraRotY*mCameraRotX;

//------------------------------------------------------------------

	// �`��J�n
	lpD3DDevice->BeginScene();

	D3DXMATRIX mView,mProj;

//------------------------------------------------------------------


	// �o�b�N�o�b�t�@�� Z �o�b�t�@���N���A
	lpD3DDevice->Clear(0,NULL,D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER,D3DCOLOR_XRGB(0,0,255),1.0f,0);

	// ���_�s��̐ݒ�
	D3DXMatrixLookAtLH(&mView,
		&CameraPos,
		&GhostPos,
		&HeadVec
	);

	// ���e�s��̐ݒ�
	D3DXMatrixPerspectiveFovLH(&mProj, D3DXToRadian(45), 4.0f/3.0f, 1.0f, 10000.0f);

	//�s��ݒ�
	lpD3DDevice->SetTransform(D3DTS_VIEW, &mView);
	lpD3DDevice->SetTransform(D3DTS_PROJECTION, &mProj);

	//�`��n�֐�
	RenderProc();

	//�X�v���C�g�֘A�̕\������
	// lpSprite->Begin(D3DXSPRITE_ALPHABLEND);
	D3DXMATRIX mWorld;
	// TIME �\��
	//RECT rcTime={80,73,135,100};
	//lpSprite->Draw(lpNumber,&rcTime,NULL,&D3DXVECTOR3(220,10,0),D3DCOLOR_ARGB(255,255,255,255));
	// lpSprite->End();

	//------------------------------------------------------------------
	//�f�o�b�O�p�ϐ�
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

	//VC++2003 �܂�
	sprintf(chscr1,"%09ld",CameraPos);
	sprintf(chscr2,"%09ld",GhostPos);
	sprintf(chscr3,"%09ld",HeadVec);
	sprintf(chscr4,"%09ld",mPlane);
	sprintf(chscr5,"%09ld",mWorld);
	sprintf(chscr6,"%09ld",Speed);
	//sprintf(chscr7,"%09ld",Num);

	//VC++2005�ȍ~
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

	// �`��I��
	lpD3DDevice->EndScene();

	// �o�b�N�o�b�t�@���v���C�}���o�b�t�@�ɃR�s�[
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

	//---------------------DirectX Graphics�֘A-----------------------

	// Direct3D �I�u�W�F�N�g���쐬
	lpD3D = Direct3DCreate9(D3D_SDK_VERSION);
	if (!lpD3D)
	{
		// �I�u�W�F�N�g�쐬���s
		MessageBox(NULL,"Direct3D �̍쐬�Ɏ��s���܂����B","ERROR",MB_OK | MB_ICONSTOP);
		// �I������
		return 0;
	}
	int adapter;

	// �g�p����A�_�v�^�ԍ�
	adapter = D3DADAPTER_DEFAULT;

	// �E�C���h�E�̍쐬�����������̂ŁADirect3D ������������
	ZeroMemory(&d3dpp,sizeof(D3DPRESENT_PARAMETERS));
	// Direct3D �������p�����[�^�̐ݒ�
	if (FULLSCREEN)
	{
		// �t���X�N���[�� : �قƂ�ǂ̃A�_�v�^�ŃT�|�[�g����Ă���t�H�[�}�b�g���g�p
		d3dpp.BackBufferFormat = D3DFMT_X8R8G8B8;
	}
	else
	{
		// �E�C���h�E : ���݂̉�ʃ��[�h���g�p
		D3DDISPLAYMODE disp;
		// ���݂̉�ʃ��[�h���擾
		lpD3D->GetAdapterDisplayMode(adapter,&disp);
		d3dpp.BackBufferFormat = disp.Format;
	}
	// �\���̈�T�C�Y�̐ݒ�
	d3dpp.BackBufferWidth = SCRW;
	d3dpp.BackBufferHeight = SCRH;
	d3dpp.SwapEffect = D3DSWAPEFFECT_FLIP;

	if (!FULLSCREEN)
	{
		// �E�C���h�E���[�h
		d3dpp.Windowed = 1;
	}

	// Z �o�b�t�@�̎����쐬
	d3dpp.EnableAutoDepthStencil = 1;
	d3dpp.AutoDepthStencilFormat = D3DFMT_D16;

	//�ޯ��ޯ̧��ۯ��\�ɂ���(GetDC���\�ɂȂ�)
	d3dpp.Flags=D3DPRESENTFLAG_LOCKABLE_BACKBUFFER;

	// �f�o�C�X�̍쐬 - T&L HAL
	if (FAILED(lpD3D->CreateDevice(adapter,D3DDEVTYPE_HAL,hwnd,D3DCREATE_HARDWARE_VERTEXPROCESSING,&d3dpp,&lpD3DDevice)))
	{
		// ���s�����̂� HAL �Ŏ��s
		if (FAILED(lpD3D->CreateDevice(adapter,D3DDEVTYPE_HAL,hwnd,D3DCREATE_SOFTWARE_VERTEXPROCESSING,&d3dpp,&lpD3DDevice)))
		{
			// ���s�����̂� REF �Ŏ��s
			if (FAILED(lpD3D->CreateDevice(adapter,D3DDEVTYPE_REF,hwnd,D3DCREATE_SOFTWARE_VERTEXPROCESSING,&d3dpp,&lpD3DDevice)))
			{
				// ���ǎ��s����
				MessageBox(NULL,"DirectX9���������ł��܂���B\n���Ή��̃p�\�R���Ǝv���܂��B","ERROR",MB_OK | MB_ICONSTOP);
				lpD3D->Release();
				// �I������
				return 0;
			}
		}
	}

	// �����_�����O�E�X�e�[�g��ݒ�
	// Z �o�b�t�@�L����->�O��֌W�̌v�Z�𐳊m�ɂ��Ă����
	lpD3DDevice->SetRenderState(D3DRS_ZENABLE,D3DZB_TRUE);
	lpD3DDevice->SetRenderState(D3DRS_ZWRITEENABLE,TRUE);

	// �A���t�@�u�����f�B���O�L����
	lpD3DDevice->SetRenderState(D3DRS_ALPHABLENDENABLE,TRUE);

	// �A���t�@�u�����f�B���O���@��ݒ�
	lpD3DDevice->SetRenderState(D3DRS_SRCBLEND,D3DBLEND_SRCALPHA);
	lpD3DDevice->SetRenderState(D3DRS_DESTBLEND,D3DBLEND_INVSRCALPHA);

	// �����_�����O���̃A���t�@�l�̌v�Z���@�̐ݒ�
	lpD3DDevice->SetTextureStageState(0,D3DTSS_ALPHAOP,D3DTOP_MODULATE);
	// �e�N�X�`���̐F���g�p
	lpD3DDevice->SetTextureStageState(0,D3DTSS_ALPHAARG1,D3DTA_TEXTURE);
	// ���_�̐F���g�p
	lpD3DDevice->SetTextureStageState(0,D3DTSS_ALPHAARG2,D3DTA_DIFFUSE);
	// �����_�����O���̐F�̌v�Z���@�̐ݒ�
	lpD3DDevice->SetTextureStageState(0,D3DTSS_COLOROP,D3DTOP_MODULATE);

	//���ʃJ�����O
	lpD3DDevice->SetRenderState(D3DRS_CULLMODE,D3DCULL_NONE);

	// �t�B���^�ݒ�
	lpD3DDevice->SetSamplerState(0,D3DSAMP_MIPFILTER,D3DTEXF_LINEAR);
	lpD3DDevice->SetSamplerState(0,D3DSAMP_MINFILTER,D3DTEXF_LINEAR);
	lpD3DDevice->SetSamplerState(0,D3DSAMP_MAGFILTER,D3DTEXF_LINEAR);

	//���C�g�̐ݒu
	D3DLIGHT9 Light;

    ZeroMemory(&Light, sizeof(D3DLIGHT9));
    Light.Type       = D3DLIGHT_DIRECTIONAL;             // �����̎��
    Light.Diffuse.r  = 1.0f;                             // �f�B�t���[�Y �q
    Light.Diffuse.g  = 1.0f;                             // �f�B�t���[�Y �f
    Light.Diffuse.b  = 1.0f;                             // �f�B�t���[�Y �a
    Light.Diffuse.a  = 0.0f;                             // �f�B�t���[�Y �`
    Light.Direction  = D3DXVECTOR3(1.0f,-1.0f,1.0f);     // ���̐i�ޕ���

    lpD3DDevice->SetLight(0, &Light);
    lpD3DDevice->LightEnable(0, TRUE);


	//���b�V���i�w�t�@�C���j�̓ǂݍ���
	LoadMesh(&Plane,"data\\AirPlane.x");	//���@
	LoadMesh(&Sky,"data\\Sky.x");			//��
	
	//�e�N�X�`���̓ǂݍ���
	LoadText(&lpSea,"data\\sea.bmp",256,256,0);		//�C
	LoadText(&lpFlash,"data\\Flash.bmp",24,24,0);	//���ˌ�(�@�e)
	LoadText(&lpGun,"data\\EPhal.bmp",16,16,D3DCOLOR_XRGB(0,0,0));	//�@�e


	//�����ǉ�
	D3DXCreateFont( lpD3DDevice , 20 , 10 , 0 , 0 , 0 , 0 , 0 , 0 , 0 , "Times New Roman" , &lpFont );
	
	//�t�H�O�̐ݒ�
	float Start = 3000.0f;
	float End   = 4000.0f;
	lpD3DDevice->SetRenderState(D3DRS_FOGTABLEMODE, D3DFOG_LINEAR);
	lpD3DDevice->SetRenderState(D3DRS_FOGSTART, *(DWORD *)(&Start));
	lpD3DDevice->SetRenderState(D3DRS_FOGEND,   *(DWORD *)(&End));
	lpD3DDevice->SetRenderState(D3DRS_FOGENABLE, TRUE);
	lpD3DDevice->SetRenderState(D3DRS_FOGCOLOR,D3DCOLOR_XRGB(200,200,200));

	srand(timeGetTime()%32767);

	//�Q�[���Ɋ֘A����ϐ��̃��Z�b�g
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

	//�Q�[���Ɋւ���I������

	timeEndPeriod(1);

	//���Z�b�g���ƂɕK�v�ȃQ�[���̏I������
	GameEnd();

	//���b�V�������
	ReleaseMesh(&Plane);
	ReleaseMesh(&Sky);


	//�e�N�X�`�������
	lpSea->Release();
	lpFlash->Release();
	lpGun->Release();


	//���������
	lpFont->Release();

	// Direct3D �I�u�W�F�N�g�����
	lpD3DDevice->Release();
	lpD3D->Release();

	return msg.wParam;
}

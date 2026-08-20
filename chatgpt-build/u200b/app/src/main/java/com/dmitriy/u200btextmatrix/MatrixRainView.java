package com.dmitriy.u200btextmatrix;

import android.content.Context;
import android.graphics.BlurMaskFilter;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.LinearGradient;
import android.graphics.Paint;
import android.graphics.RectF;
import android.graphics.Shader;
import android.graphics.Typeface;
import android.util.AttributeSet;
import android.view.View;
import java.util.Random;

public final class MatrixRainView extends View {
    private static final char[] GLYPHS=("0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZｱｲｳｴｵｶｷｸｹｺｻｼｽｾｿﾀﾁﾂﾃﾄﾅﾆﾇﾈﾉﾊﾋﾌﾍﾎﾏﾐﾑﾒﾓﾔﾕﾖﾗﾘﾙﾚﾛﾜ$+-=<>#*:").toCharArray();
    private static final class Stream{float x,y,speed,brightness;int length,seed;}
    private final Paint glyph=new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint softGlyph=new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint bg=new Paint();
    private final Paint veil=new Paint();
    private final RectF softZone=new RectF();
    private final Random rnd=new Random(200);
    private Stream[] streams=new Stream[0];
    private float step,size,density;
    private long last;

    public MatrixRainView(Context c){super(c);init();}
    public MatrixRainView(Context c, AttributeSet a){super(c,a);init();}

    private void init(){
        density=getResources().getDisplayMetrics().density;
        size=14*density;step=17*density;
        glyph.setTypeface(Typeface.create("monospace",Typeface.NORMAL));glyph.setTextSize(size);glyph.setTextAlign(Paint.Align.CENTER);
        softGlyph.setTypeface(Typeface.create("monospace",Typeface.NORMAL));softGlyph.setTextSize(size);softGlyph.setTextAlign(Paint.Align.CENTER);
        softGlyph.setMaskFilter(new BlurMaskFilter(Math.max(1f,.9f*density),BlurMaskFilter.Blur.NORMAL));
        veil.setColor(Color.argb(24,0,8,4));
        setImportantForAccessibility(IMPORTANT_FOR_ACCESSIBILITY_NO);
    }

    public void setSoftZone(float left,float top,float right,float bottom){softZone.set(left,top,right,bottom);invalidate();}

    @Override protected void onSizeChanged(int w,int h,int ow,int oh){
        if(w<=0||h<=0)return;
        // 3.5: roughly twice as many live Matrix columns as 3.4.
        int count=Math.max(44,(int)(w/(9.5f*density)));
        streams=new Stream[count];
        for(int i=0;i<count;i++){
            Stream s=new Stream();s.x=(i+.5f)*w/count+rr(-3*density,3*density);s.y=rr(-h,h);
            s.speed=rr(82*density,185*density);s.length=ri(24,56);s.brightness=rr(.68f,1f);s.seed=rnd.nextInt(10000);streams[i]=s;
        }
        last=0;
    }

    @Override protected void onDraw(Canvas c){
        int w=getWidth(),h=getHeight();if(w<=0||h<=0)return;
        bg.setShader(new LinearGradient(0,0,0,h,Color.rgb(1,11,5),Color.rgb(0,3,2),Shader.TileMode.CLAMP));c.drawRect(0,0,w,h,bg);bg.setShader(null);
        long now=System.nanoTime();float dt=last==0?.016f:Math.min(.045f,(now-last)/1_000_000_000f);last=now;
        float center=w*.5f,half=w*.34f;
        for(Stream s:streams){
            s.y+=s.speed*dt;
            if(s.y-s.length*step>h+step){s.y=rr(-h*.65f,-step);s.speed=rr(82*density,185*density);s.length=ri(24,56);s.brightness=rr(.68f,1f);s.seed=rnd.nextInt(10000);}
            float factor=.56f+.44f*Math.min(1f,Math.abs(s.x-center)/Math.max(1f,half));
            for(int i=0;i<s.length;i++){
                float y=s.y-i*step;if(y<-step||y>h+step)continue;
                float tail=1f-i/(float)s.length;int alpha=clamp((int)(tail*tail*205*factor*s.brightness),13,205);
                char ch=GLYPHS[Math.floorMod(s.seed+i*17+(int)(s.y/step),GLYPHS.length)];
                boolean soft=!softZone.isEmpty()&&softZone.contains(s.x,y);Paint p=soft?softGlyph:glyph;int a=soft?clamp((int)(alpha*.66f),16,145):alpha;
                if(i==0){p.setColor(Color.argb(soft?clamp((int)(160*factor),65,160):clamp((int)(235*factor),95,235),220,255,224));p.setShadowLayer(size*(soft?.18f:.40f),0,0,Color.rgb(90,255,135));}
                else if(i<=3){p.setColor(Color.argb(a,125,255,155));p.setShadowLayer(size*(soft?.08f:.15f),0,0,Color.rgb(25,225,92));}
                else{p.setColor(Color.argb(a,45,224,91));p.clearShadowLayer();}
                c.drawText(String.valueOf(ch),s.x,y,p);
            }
        }
        glyph.clearShadowLayer();softGlyph.clearShadowLayer();
        c.drawRoundRect(w*.08f,h*.04f,w*.92f,h*.96f,w*.025f,w*.025f,veil);
        postInvalidateOnAnimation();
    }

    private float rr(float a,float b){return a+rnd.nextFloat()*(b-a);}private int ri(int a,int b){return a+rnd.nextInt(b-a+1);}private static int clamp(int v,int a,int b){return Math.max(a,Math.min(b,v));}
}

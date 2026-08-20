package com.dmitriy.u200btextmatrix;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.LinearGradient;
import android.graphics.Paint;
import android.graphics.Shader;
import android.graphics.Typeface;
import android.util.AttributeSet;
import android.view.View;
import java.util.Random;

public final class MatrixRainView extends View {
    private static final char[] GLYPHS = ("0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZｱｲｳｴｵｶｷｸｹｺｻｼｽｾｿﾀﾁﾂﾃﾄﾅﾆﾇﾈﾉﾊﾋﾌﾍﾎﾏﾐﾑﾒﾓﾔﾕﾖﾗﾘﾙﾚﾛﾜ$+-=<>#*:").toCharArray();
    private static final class Stream { float x,y,speed,brightness; int length,seed; }
    private final Paint glyph = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint bg = new Paint();
    private final Paint veil = new Paint();
    private final Random rnd = new Random(200);
    private Stream[] streams = new Stream[0];
    private float step,size;
    private long last;

    public MatrixRainView(Context c){super(c);init();}
    public MatrixRainView(Context c, AttributeSet a){super(c,a);init();}
    private void init(){
        float d=getResources().getDisplayMetrics().density;
        size=14*d; step=17*d;
        glyph.setTypeface(Typeface.create("monospace",Typeface.NORMAL));
        glyph.setTextSize(size); glyph.setTextAlign(Paint.Align.CENTER);
        veil.setColor(Color.argb(130,0,8,4));
        setImportantForAccessibility(IMPORTANT_FOR_ACCESSIBILITY_NO);
    }
    @Override protected void onSizeChanged(int w,int h,int ow,int oh){
        if(w<=0||h<=0)return;
        float d=getResources().getDisplayMetrics().density;
        int count=Math.max(20,(int)(w/(20*d)));
        streams=new Stream[count];
        for(int i=0;i<count;i++){
            Stream s=new Stream();
            s.x=(i+.5f)*w/count+rr(-4*d,4*d); s.y=rr(-h,h);
            s.speed=rr(85*d,175*d); s.length=ri(20,48); s.brightness=rr(.7f,1f); s.seed=rnd.nextInt(10000); streams[i]=s;
        }
        last=0;
    }
    @Override protected void onDraw(Canvas c){
        int w=getWidth(),h=getHeight(); if(w<=0||h<=0)return;
        bg.setShader(new LinearGradient(0,0,0,h,Color.rgb(1,11,5),Color.rgb(0,3,2),Shader.TileMode.CLAMP));
        c.drawRect(0,0,w,h,bg); bg.setShader(null);
        long now=System.nanoTime(); float dt=last==0?.016f:Math.min(.045f,(now-last)/1_000_000_000f); last=now;
        float center=w*.5f, half=w*.34f;
        for(Stream s:streams){
            s.y+=s.speed*dt;
            if(s.y-s.length*step>h+step){s.y=rr(-h*.65f,-step);s.speed=rr(85,175)*getResources().getDisplayMetrics().density;s.length=ri(20,48);s.seed=rnd.nextInt(10000);}
            float factor=.25f+.75f*Math.min(1f,Math.abs(s.x-center)/Math.max(1f,half));
            for(int i=0;i<s.length;i++){
                float y=s.y-i*step; if(y<-step||y>h+step)continue;
                float tail=1f-i/(float)s.length; int alpha=clamp((int)(tail*tail*210*factor*s.brightness),12,210);
                char ch=GLYPHS[Math.floorMod(s.seed+i*17+(int)(s.y/step),GLYPHS.length)];
                if(i==0){glyph.setColor(Color.argb(clamp((int)(235*factor),70,235),220,255,224));glyph.setShadowLayer(size*.42f,0,0,Color.rgb(90,255,135));}
                else if(i<=3){glyph.setColor(Color.argb(alpha,125,255,155));glyph.setShadowLayer(size*.16f,0,0,Color.rgb(25,225,92));}
                else {glyph.setColor(Color.argb(alpha,45,224,91));glyph.clearShadowLayer();}
                c.drawText(String.valueOf(ch),s.x,y,glyph);
            }
        }
        glyph.clearShadowLayer();
        c.drawRoundRect(w*.12f,h*.06f,w*.88f,h*.93f,w*.035f,w*.035f,veil);
        postInvalidateOnAnimation();
    }
    private float rr(float a,float b){return a+rnd.nextFloat()*(b-a);} private int ri(int a,int b){return a+rnd.nextInt(b-a+1);} private static int clamp(int v,int a,int b){return Math.max(a,Math.min(b,v));}
}

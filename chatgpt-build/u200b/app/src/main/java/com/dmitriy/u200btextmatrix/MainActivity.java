package com.dmitriy.u200btextmatrix;

import android.app.Activity;
import android.content.ClipData;
import android.content.ClipboardManager;
import android.content.Context;
import android.content.Intent;
import android.graphics.Color;
import android.graphics.Typeface;
import android.graphics.drawable.GradientDrawable;
import android.net.Uri;
import android.os.Bundle;
import android.view.Gravity;
import android.view.View;
import android.widget.Button;
import android.widget.CheckBox;
import android.widget.EditText;
import android.widget.FrameLayout;
import android.widget.LinearLayout;
import android.widget.TextView;
import android.widget.Toast;
import java.io.IOException;
import java.io.OutputStream;
import java.io.OutputStreamWriter;
import java.nio.charset.StandardCharsets;

public final class MainActivity extends Activity {
    private static final int SAVE_REQUEST=2001;
    private EditText editor;
    private CheckBox debug;
    private TextView status;
    private MatrixRainView rain;
    private FrameLayout root;
    private String actual="";
    private boolean internal;
    private final int GREEN=Color.rgb(57,255,136),TEXT=Color.rgb(228,255,236),DIM=Color.rgb(120,184,143);

    @Override protected void onCreate(Bundle b){
        super.onCreate(b);
        getWindow().setStatusBarColor(Color.rgb(1,4,2));
        getWindow().setNavigationBarColor(Color.rgb(1,4,2));
        buildUi();
        if(b!=null){actual=b.getString("actual","");if(!actual.isEmpty())refresh();}
    }

    private int dp(float v){return Math.round(v*getResources().getDisplayMetrics().density);}
    private GradientDrawable box(int fill,int stroke,float radius){GradientDrawable g=new GradientDrawable();g.setColor(fill);g.setCornerRadius(dp(radius));g.setStroke(dp(1),stroke);return g;}
    private TextView text(String s,int sp,int color,boolean mono){TextView v=new TextView(this);v.setText(s);v.setTextColor(color);v.setTextSize(sp);if(mono)v.setTypeface(Typeface.MONOSPACE);return v;}
    private Button button(String s,boolean primary){
        Button b=new Button(this);b.setText(s);b.setTextColor(primary?GREEN:TEXT);b.setTextSize(primary?12:10);b.setTypeface(Typeface.MONOSPACE,primary?Typeface.BOLD:Typeface.NORMAL);b.setAllCaps(false);b.setGravity(Gravity.CENTER);
        int fill=primary?Color.argb(150,13,59,34):Color.argb(108,6,17,10);
        b.setBackground(box(fill,primary?GREEN:Color.rgb(33,216,107),5));b.setMinHeight(0);b.setPadding(dp(5),0,dp(5),0);return b;
    }

    private void buildUi(){
        root=new FrameLayout(this);root.setBackgroundColor(Color.rgb(2,6,4));
        rain=new MatrixRainView(this);root.addView(rain,new FrameLayout.LayoutParams(-1,-1));
        LinearLayout main=new LinearLayout(this);main.setOrientation(LinearLayout.VERTICAL);main.setPadding(dp(16),dp(12),dp(16),dp(9));root.addView(main,new FrameLayout.LayoutParams(-1,-1));

        TextView kicker=text("ZERO-WIDTH PROCESSOR // LIVE DIGITAL RAIN",9,DIM,true);main.addView(kicker,new LinearLayout.LayoutParams(-1,-2));
        TextView title=text("TEXT MATRIX // U+200B",25,GREEN,false);title.setTypeface(Typeface.create("sans-serif-condensed",Typeface.BOLD));LinearLayout.LayoutParams tp=new LinearLayout.LayoutParams(-1,-2);tp.topMargin=dp(2);main.addView(title,tp);
        TextView desc=text("Matrix rain идёт через весь экран. В поле текста он мягко размывается, чтобы не мешать чтению.",11,Color.rgb(145,184,157),true);LinearLayout.LayoutParams dpv=new LinearLayout.LayoutParams(-1,-2);dpv.topMargin=dp(7);main.addView(desc,dpv);

        LinearLayout editPanel=new LinearLayout(this);editPanel.setOrientation(LinearLayout.VERTICAL);editPanel.setPadding(dp(9),dp(8),dp(9),dp(7));editPanel.setBackground(box(Color.argb(112,6,17,10),Color.argb(170,33,216,107),8));LinearLayout.LayoutParams ep=new LinearLayout.LayoutParams(-1,0,1f);ep.topMargin=dp(9);main.addView(editPanel,ep);
        TextView label=text("SOURCE / RESULT",10,GREEN,true);label.setTypeface(Typeface.MONOSPACE,Typeface.BOLD);editPanel.addView(label,new LinearLayout.LayoutParams(-1,-2));
        editor=new EditText(this);editor.setTextColor(TEXT);editor.setHintTextColor(DIM);editor.setHint("Текст песни / промт…");editor.setTextSize(16);editor.setTypeface(Typeface.MONOSPACE);editor.setGravity(Gravity.TOP|Gravity.START);editor.setPadding(dp(11),dp(10),dp(11),dp(10));editor.setBackground(box(Color.argb(88,6,17,10),Color.rgb(33,216,107),7));editor.setInputType(android.text.InputType.TYPE_CLASS_TEXT|android.text.InputType.TYPE_TEXT_FLAG_MULTI_LINE|android.text.InputType.TYPE_TEXT_FLAG_NO_SUGGESTIONS);LinearLayout.LayoutParams edp=new LinearLayout.LayoutParams(-1,0,1f);edp.topMargin=dp(6);editPanel.addView(editor,edp);
        debug=new CheckBox(this);debug.setText("DEBUG // показывать U+200B как *");debug.setTextColor(Color.rgb(145,184,157));debug.setTextSize(10);debug.setTypeface(Typeface.MONOSPACE);debug.setChecked(true);editPanel.addView(debug,new LinearLayout.LayoutParams(-1,-2));

        LinearLayout controls=new LinearLayout(this);controls.setOrientation(LinearLayout.VERTICAL);controls.setPadding(dp(8),dp(8),dp(8),dp(8));controls.setBackground(box(Color.argb(118,6,17,10),Color.argb(165,33,216,107),8));LinearLayout.LayoutParams cp=new LinearLayout.LayoutParams(-1,-2);cp.topMargin=dp(7);main.addView(controls,cp);
        Button convert=button("ВСТАВИТЬ U+200B",true);controls.addView(convert,new LinearLayout.LayoutParams(-1,dp(46)));
        LinearLayout row1=new LinearLayout(this);row1.setOrientation(LinearLayout.HORIZONTAL);LinearLayout.LayoutParams rp=new LinearLayout.LayoutParams(-1,dp(40));rp.topMargin=dp(6);controls.addView(row1,rp);Button copy=button("КОПИРОВАТЬ",false),save=button("СОХРАНИТЬ TXT",false);row1.addView(copy,new LinearLayout.LayoutParams(0,-1,1));row1.addView(new View(this),new LinearLayout.LayoutParams(dp(7),1));row1.addView(save,new LinearLayout.LayoutParams(0,-1,1));
        LinearLayout row2=new LinearLayout(this);row2.setOrientation(LinearLayout.HORIZONTAL);LinearLayout.LayoutParams rp2=new LinearLayout.LayoutParams(-1,dp(40));rp2.topMargin=dp(6);controls.addView(row2,rp2);Button remove=button("УДАЛИТЬ U+200B",false),clear=button("ОЧИСТИТЬ",false);row2.addView(remove,new LinearLayout.LayoutParams(0,-1,1));row2.addView(new View(this),new LinearLayout.LayoutParams(dp(7),1));row2.addView(clear,new LinearLayout.LayoutParams(0,-1,1));
        status=text("SYSTEM READY // результат появится в этом же поле.",10,DIM,true);status.setPadding(dp(9),dp(7),dp(9),dp(7));status.setBackground(box(Color.argb(96,1,8,5),Color.argb(110,33,216,107),5));LinearLayout.LayoutParams stp=new LinearLayout.LayoutParams(-1,-2);stp.topMargin=dp(7);controls.addView(status,stp);
        setContentView(root);

        convert.setOnClickListener(v->convert());copy.setOnClickListener(v->copy());save.setOnClickListener(v->save());remove.setOnClickListener(v->remove());clear.setOnClickListener(v->{actual="";setEditor("");status.setText("SYSTEM // OK");});debug.setOnCheckedChangeListener((b,c)->refresh());
        editor.addTextChangedListener(new android.text.TextWatcher(){public void beforeTextChanged(CharSequence s,int st,int c,int a){}public void afterTextChanged(android.text.Editable e){}public void onTextChanged(CharSequence s,int st,int b,int c){if(!internal&&!actual.isEmpty()){actual="";status.setText("SYSTEM // текст изменён; обработайте снова.");}}});
        editor.addOnLayoutChangeListener((v,l,t,r,b,ol,ot,or_,ob)->updateSoftZone());
        root.addOnLayoutChangeListener((v,l,t,r,b,ol,ot,or_,ob)->updateSoftZone());
        root.post(this::updateSoftZone);
    }

    private void updateSoftZone(){
        if(rain==null||editor==null||editor.getWidth()<=0||editor.getHeight()<=0)return;
        int[] a=new int[2],b=new int[2];rain.getLocationOnScreen(a);editor.getLocationOnScreen(b);float inset=dp(4);
        float left=b[0]-a[0]+inset,top=b[1]-a[1]+inset,right=left+editor.getWidth()-2*inset,bottom=top+editor.getHeight()-2*inset;
        rain.setSoftZone(left,top,right,bottom);
    }

    private void convert(){String src=actual.isEmpty()?editor.getText().toString():TextProcessor.remove(actual);if(src.isEmpty()){status.setText("SYSTEM // поле пустое.");return;}TextProcessor.Result r=TextProcessor.insert(src);actual=r.text;refresh();status.setText("SYSTEM // обработано. Добавлено U+200B: "+r.count);}
    private void refresh(){if(actual.isEmpty())return;setEditor(debug.isChecked()?TextProcessor.debug(actual):actual);}
    private void setEditor(String s){internal=true;editor.setText(s);editor.setSelection(editor.length());internal=false;}
    private String real(){return actual.isEmpty()?editor.getText().toString():actual;}
    private void copy(){String s=real();if(s.isEmpty())return;ClipboardManager c=(ClipboardManager)getSystemService(Context.CLIPBOARD_SERVICE);c.setPrimaryClip(ClipData.newPlainText("U+200B MATRIX",s));status.setText("SYSTEM // текст скопирован.");Toast.makeText(this,"Текст скопирован",Toast.LENGTH_SHORT).show();}
    private void remove(){String s=TextProcessor.remove(real());actual="";setEditor(s);status.setText("SYSTEM // U+200B удалены.");}
    private void save(){if(real().isEmpty())return;Intent i=new Intent(Intent.ACTION_CREATE_DOCUMENT);i.addCategory(Intent.CATEGORY_OPENABLE);i.setType("text/plain");i.putExtra(Intent.EXTRA_TITLE,"song_with_U200B.txt");startActivityForResult(i,SAVE_REQUEST);}
    @Override protected void onActivityResult(int req,int result,Intent data){super.onActivityResult(req,result,data);if(req==SAVE_REQUEST&&result==RESULT_OK&&data!=null&&data.getData()!=null){Uri u=data.getData();try(OutputStream os=getContentResolver().openOutputStream(u,"w");OutputStreamWriter w=new OutputStreamWriter(os,StandardCharsets.UTF_8)){w.write('\uFEFF');w.write(real());status.setText("SYSTEM // TXT сохранён UTF-8.");}catch(IOException e){status.setText("SYSTEM // ошибка сохранения: "+e.getMessage());}}}
    @Override protected void onSaveInstanceState(Bundle b){b.putString("actual",actual);super.onSaveInstanceState(b);}
}

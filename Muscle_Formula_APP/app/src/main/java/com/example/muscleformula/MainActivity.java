package com.example.muscleformula;

import androidx.appcompat.app.AppCompatActivity;

import android.content.Intent;
import android.os.Bundle;
import android.view.View;
import android.widget.ArrayAdapter;
import android.widget.Button;
import android.widget.Spinner;

import java.util.ArrayList;
import java.util.Arrays;

public class MainActivity extends AppCompatActivity {

    Spinner sp_select, sp_connect;
    Button btn_record;


    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        sp_select = findViewById(R.id.sp_select);
        sp_connect = findViewById(R.id.sp_connect);
        btn_record = findViewById(R.id.btn_record);

        btn_record.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                Intent intent = new Intent(MainActivity.this, RecordActivity.class);
                startActivity(intent);
            }
        });

        String[] value = {"Dumbel-low (덤벨로우)", "Pull-Up (턱걸이)", "Plank (플랭크)", "Sit-up (윗몸일으키기)", "Push-Up (팔굽혀펴기)"};
        value[0]="운동 종류 선택";
        ArrayList<String> arrayList = new ArrayList<>(Arrays.asList(value));
        ArrayAdapter<String> arrayAdapter = new ArrayAdapter<>(this, R.layout.style_spinner,arrayList);
        sp_select.setAdapter(arrayAdapter);

        String[] value2 = {"SC-004", "SC-008"};
        value[0]="기기 연결";
        ArrayList<String> arrayList2 = new ArrayList<>(Arrays.asList(value2));
        ArrayAdapter<String> arrayAdapter2 = new ArrayAdapter<>(this, R.layout.style_spinner,arrayList2);
        sp_connect.setAdapter(arrayAdapter2);
    }
}
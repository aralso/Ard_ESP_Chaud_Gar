
#include "variables.h"

// Variables pour le PID
double Consigne, Input, Output;
double Kp = 1.7, Ki = 0.5, Kd = 0.5;
uint16_t Consigne_G, Consigne_HG;
#define MIN_MAX 5
uint8_t min_max_pid;

// Loi d'eau
int8_t Pt1;
uint8_t Pt2, Pt1Val, Pt2Val;

uint8_t HG, Ballon, MMCh;
uint8_t mode_pid;

// graphique cycles compresseur
uint8_t etat_compr;
unsigned long milli_marche, milli_arret;
uint16_t dernier_fct;
uint16_t heure_arret;
uint16_t T_evapo_min=700;  // +30*10   -30 à 30 : -30°C=0  0°C=300 10°C=400
uint16_t Seuil_T_Evapo;  // Seuil T Evapo pour mettre à l'arret la PAC avant la mise en sécurité

PID myPID(&Input, &Output, &Consigne, Kp, Ki, Kd, DIRECT);

#ifdef Temp_int_DS18B20
  // Capteur temperature Dallas DS18B20  Temperature intérieure
  typedef uint8_t DeviceAddress[8];
  const int PIN_Tint = 13;      // Tint:Entrée onewire GPIO DS18B20
  //OneWire oneWire(PIN_Tint);
  //DallasTemperature ds(&oneWire);
  int nb_capteurs_temp = 1;  //DS18B20
  DeviceAddress Thermometer[5];
  DeviceAddress adds;
#endif

DHT dht[] = {
  { PIN_Tint22, DHT22 },
};

// Temperature intérieure
float Tint;
float loi_eau_Tint;

float T_obj;
float T_loi_eau;

// Entrée analogique pour mesurer la température exterieure
uint16_t Text1, Text2, Text1Val, Text2Val;  // valeurs pour calibration (T*10)
float Text;
#define resolutionADC 4095
#define TBETA 3950  // Coefficient Beta de la thermistance
uint16_t TBeta;
#define R0E 10000  // Résistance à 25° pour une NTC10K
uint16_t Therm0;

// Sortie analogique vers PAC
uint16_t S_analo_min;  // voltage pour -5°C
uint16_t S_analo_max;  // voltage pour 20°C
float TPAC = 18;
float TRef = 18;

#define TPACMIN 10
#define TPACMAX 30
uint8_t TPacMin, TPacMax;

// Régulation
#define MODE 1  // 1:PAC 2:radiateur 3:PAC+Radiateur
uint8_t Mode;



uint16_t periode_lecture_temp = 1;  // 60, en minutes

uint8_t init_nb_heures = 0;
uint8_t init_heure_max = 0;
float init_temp_max = -20;
uint8_t init_heure_min = 0;
float init_temp_min = 40;

float loi_deau(float temp_ext, float temp_cons, float *Tloi);

#ifdef Temp_int_DS18B20
  void printAddress(DeviceAddress deviceAddress) {
    for (uint8_t i = 0; i < 8; i++) {
      Serial.print("0x");
      if (deviceAddress[i] < 0x10) Serial.print("0");
      Serial.print(deviceAddress[i], HEX);
      if (i < 7) Serial.print(", ");
    }
    Serial.println("");
  }
#endif

void init_10_secondes()
{
}

//setup au debut
void setup_0()
{

  if (NB_Graphique)
  {
    graphique[0][0] = 180;  //int - vert
    graphique[1][0] = 185;
    //graphique[2][0] = 190;
    //graphique[3][0] = 185;
    //graphique[4][0] = 180;
    //graphique[5][0] = 185;
  }
  /*graphique[0][1] = 110;  // ext - bleu
  graphique[1][1] = 80;
  graphique[2][1] = 103;
  graphique[3][1] = 95;
  graphique[0][2] = 245;  // eau - rouge
  graphique[1][2] = 253;
  graphique[2][2] = 273;
  graphique[3][2] = 255;*/

  if (NB_Graphique==6)
  {
    /*graphique[0][3] = 35; 
    graphique[1][3] = 38;
    graphique[2][3] = 42;
    graphique[3][3] = 32;
    graphique[0][4] = 50;  // divisé par 10
    graphique[1][4] = 55;
    graphique[2][4] = 48;  
    graphique[3][4] = 52;*/
  }
}

// setup : lecture nvs
void setup_nvs()
{

    // Initialisation des valeurs de PID
  Kp = (float)preferences_nvs.getUShort("Kp", 0) / 100;    //  *100
  Ki = (float)preferences_nvs.getUShort("Ki", 0) / 10000;  //  *10000
  Kd = (float)preferences_nvs.getUShort("Kd", 0) / 10000;  //  *10000
  uint8_t err_coeff = 0;
  if ((!Kp)) err_coeff = 1;
  if (Kp > 2) err_coeff = 1;
  if (err_coeff) {
    Serial.println("PID: nouvelles valeurs par défaut");
    Kp = 1.66;
    Ki = 2;
    Kd = 0.1;
    preferences_nvs.putUShort("Kp", 166);  // *100
    preferences_nvs.putUShort("Ki", 10000);  // *10000
    preferences_nvs.putUShort("Kd", 1000);  // *10000
    Serial.printf("Raz PID: Kp:%.2f  Ki:%.4f  Kd:%.4f\n\r", Kp, Ki, Kd);
  }
  else
    Serial.printf("PID: Kp:%.2f  Ki:%.4f  Kd:%.4f\n\r", Kp, Ki, Kd);
  myPID.SetTunings(Kp, Ki, Kd);


  // periode du cycle
  periode_cycle = preferences_nvs.getUChar("cycle", 0);  // de 10 a 60
  if ((periode_cycle < 10) || (periode_cycle > 60)) {
    periode_cycle = 15;
    preferences_nvs.putUChar("cycle", periode_cycle);
    Serial.printf("Raz periode cycle : val par defaut %imin\n\r", periode_cycle);
  }
  else Serial.printf("periode cycle : %imin\n", periode_cycle);

  mode_rapide = preferences_nvs.getUChar("Rap", 0);  // mode=12 => mode_rapide
  if ((mode_rapide) && (mode_rapide != 12)) {
    mode_rapide=0;
    preferences_nvs.putUChar("Rap", 0);
    Serial.println("Raz Mode rapide:0");
  }
  else
    Serial.printf("Mode rapide : %i\n\r", mode_rapide);

  /*// valeurs de calibration sonde temperature exterieure
  TBeta = preferences_nvs.getUShort("Beta", 0);   // 3950
  if ((TBeta<3000) || (TBeta>5000))
  {
    TBeta = TBETA;  // 3950
    Serial.printf("Calib T.Ext: nouveau Béta:%i\n", TBeta);
    preferences_nvs.putUShort("Beta", TBeta);   // 
  }

  Therm0 =  preferences_nvs.getUShort("R0", 0);   // 10000
  if ((Therm0 < 7000) || (Therm0 > 13000))
  {
    Therm0 = R0E;  // 10000
    Serial.printf("Calib T.Ext: nouveau R0:%i\n", Therm0);
    preferences_nvs.putUShort("R0", Therm0);   // 
  }*/

  // Lecture des temperatures PAC minimum et Maximum
  TPacMin = preferences_nvs.getUChar("Tmin", 0);  // 10°C
  TPacMax = preferences_nvs.getUChar("Tmax", 0);  // 30°C
  if ((TPacMin < 5) || (TPacMin > 30) || (TPacMax < 15) || (TPacMax > 40) || (TPacMin >= TPacMax)) {
    TPacMin = TPACMIN;
    TPacMax = TPACMAX;
    Serial.printf("Raz Min-Max: %i°C  %i°C\n", TPacMin, TPacMax);
    preferences_nvs.putUChar("Tmin", TPacMin);  //
    preferences_nvs.putUChar("Tmax", TPacMax);  //
  } else
    Serial.printf("TPAC Min-Max: %i°C  %i°C\n", TPacMin, TPacMax);


  /* Text1 = preferences_nvs.getUShort("T1", 0);   // 10°C  *10
  Text2 = preferences_nvs.getUShort("T2", 0);   // 20°C
  Text1Val = preferences_nvs.getUShort("T1V", 0); // 500
  Text2Val = preferences_nvs.getUShort("T2V", 0); // 2000
  uint8_t err_calib_sonde=0;
  if ((Text1<10) || (Text1>200)) err_calib_sonde=1;  // 1°C à 20°C
  if ((Text2<150) || (Text2>300)) err_calib_sonde=1; // 15°C à 30°C
  if ((Text1>Text2) || (Text2-Text1<50)) err_calib_sonde=1; // au moins 5°C d'écart
  if ((Text1Val<Text2Val) || (Text1Val-Text2Val<200)) err_calib_sonde=1; // au moins 200 d'écart
  if (err_calib_sonde)
  {
    Serial.println("Calib T.Ext: nouvelles valeurs par défaut");
    Text1 = 70;  // 7°C
    Text2 = 250;  // 25°C
    Text1Val = 2673;
    Text2Val = 1630;
    preferences_nvs.putUShort("T1", Text1);   // 
    preferences_nvs.putUShort("T2", Text2);   // 
    preferences_nvs.putUShort("T1V", Text1Val); // 
    preferences_nvs.putUShort("T2V", Text2Val); // 
  } 
  Serial.printf("Calib T.Ext: Point1: %i pour %.1f°C   Point2: %i°C -> %.1f°C\n", Text1Val, (float)Text1/10, Text2Val, (float)Text2/10) ;*/

  // Mode regulation : 1:normal, 2:loi d'eau avec Text, 3:fixe
  mode_pid = preferences_nvs.getUChar("PID", 0);
  if ((!mode_pid) || (mode_pid > 3)) {
    mode_pid = 1;
    preferences_nvs.putUChar("PID", mode_pid);
    Serial.printf("Raz mode PID:%i\n\r", mode_pid);
  } else
    Serial.printf("mode PID:%i\n\r", mode_pid);

  TRef = (float)preferences_nvs.getUShort("TRef", 0) / 10;  // pour le cas de mode_pid=fixe
  if ((TRef < 10) || (TRef > 30)) {
    TRef = 18.0;
    preferences_nvs.putUShort("TRef", 180);
    Serial.printf("Raz TRef:%f\n", TRef);
  } else
    Serial.printf("TRef=%f\n", TRef);



  Seuil_T_Evapo = (float)preferences_nvs.getUShort("TEv", 0);  // Seuil arret T_evapo
  if ((Seuil_T_Evapo < 100) || (Seuil_T_Evapo > 300)) {   // -20°C à 0°C
    Seuil_T_Evapo = 200;   // -10°C
    preferences_nvs.putUShort("TEv", 100);
    Serial.printf("Raz Seuil T. Evapo:%f°C\n", ((float)(Seuil_T_Evapo)-300)/10);
  } else
    Serial.printf("Seuil T Evapo=%f°C\n", ((float)(Seuil_T_Evapo)-300)/10);


  Mode = preferences_nvs.getUChar("Mode", 0);
  if ((!Mode) || (Mode > 3)) {
    Mode = MODE;
    preferences_nvs.putUChar("Mode", Mode);
    Serial.printf("Raz Mode:%i\n", Mode);
  } else
    Serial.printf("Mode :%i\n", Mode);

  // valeurs de la loi d'eau
  Pt1 = preferences_nvs.getUChar("Pt1", 0) - 30;  // -5°C +30 =>25
  Pt1Val = preferences_nvs.getUChar("Pt1V", 0);   // 35°C
  Pt2 = preferences_nvs.getUChar("Pt2", 0);       // 20°C
  Pt2Val = preferences_nvs.getUChar("Pt2V", 0);   // 20°C
  uint8_t err_loi_eau = 0;
  if ((Pt1 < -10) || (Pt1 > 15)) err_loi_eau = 1;                   // -10°C à 15°C
  if ((Pt2 < 10) || (Pt2 > 30)) err_loi_eau = 1;                    // 10°C à 30°C
  if ((Pt1 > Pt2) || (Pt2 - Pt1 < 10)) err_loi_eau = 1;             // au moins 10°C d'écart
  if ((Pt1Val < Pt2Val) || (Pt1Val - Pt2Val < 5)) err_loi_eau = 1;  // au moins 5° d'écart
  if (err_loi_eau) {
    Serial.println("Loi d'eau: nouvelles valeurs par défaut");
    Pt1 = -5;     // -5°C
    Pt1Val = 35;  // 35°C
    Pt2 = 20;     // 20°C
    Pt2Val = 20;  // 20°C
    preferences_nvs.putUChar("Pt1", Pt1 + 30);
    preferences_nvs.putUChar("Pt1V", Pt1Val);
    preferences_nvs.putUChar("Pt2", Pt2);
    preferences_nvs.putUChar("Pt2V", Pt2Val);
    Serial.printf("Raz Loi d'eau: Point1: %i°C -> %i°C   Point2: %i pour %i°C\n\r", Pt1, Pt1Val, Pt2, Pt2Val);
  }
  else
    Serial.printf("Loi d'eau: Point1: %i°C -> %i°C   Point2: %i pour %i°C\n\r", Pt1, Pt1Val, Pt2, Pt2Val);



  min_max_pid = preferences_nvs.getUChar("MPid", 0);
  if ((min_max_pid < 2) || (min_max_pid > 10)) {
    min_max_pid = MIN_MAX;  //5
    preferences_nvs.putUChar("MPid", min_max_pid);
    Serial.printf("Raz Min_max_Pid: %i\n", min_max_pid);
  }
  else
    Serial.printf("Min_max_Pid: %i\n", min_max_pid);


  /*// Initialisation des variables de réglage pour la sortie analogique-PWM PAC
  S_analo_min = preferences_nvs.getUShort("Smin", 0);   
  S_analo_max = preferences_nvs.getUShort("Smax", 0);   
  uint8_t err_s_analog=0;
  if ((S_analo_min<S_analo_max) || (S_analo_min - S_analo_max<800)) err_s_analog=1;
  if ((!S_analo_min) || (!S_analo_max)) err_s_analog=1;
  if (err_s_analog)
  {
    Serial.println("Réglage sortie anal-PWM PAC: nouvelles valeurs par défaut");
    S_analo_min = 1730;
    S_analo_max = 248;
    preferences_nvs.putUShort("Smin", S_analo_min);
    preferences_nvs.putUShort("Smax", S_analo_max);
  } 
  Serial.printf("PWM PAC: -10°C:%i   30°C:%i\n", S_analo_min, S_analo_max) ;*/

  // Initialisation des variables de consignes/HG/ballon/MMC
  Consigne_G = preferences_nvs.getUChar("Cons", 0);   //  *10
  MMCh = preferences_nvs.getUChar("MMC", 0);   //  *10
  Consigne_HG = preferences_nvs.getUChar("C_HG", 0);  //  *10
  HG = preferences_nvs.getUChar("HG", 0);
  Ballon = preferences_nvs.getUChar("Bal", 0);
  if ((Consigne_G < 130) || (Consigne_G > 220))  // entre 13°C et 22°C
  {
    Serial.println("Raz consigne :  valeur par defaut:15°C");
    Consigne_G = 150;  //15°C
    preferences_nvs.putUChar("Cons", Consigne_G);
  }
  if ((Consigne_HG < 80) || (Consigne_HG > 160))  // entre 8°C et 16°C
  {
    Serial.println("consigne Hors-gel: nouvelle valeur par defaut");
    Consigne_HG = 120;  //12°C
    preferences_nvs.putUChar("C_HG", Consigne_HG);
  }
  if ((!HG) || (HG > 2))  // 1:non 2:HG
  {
    Serial.println("HG : non actif par defaut");
    HG = 1;
    preferences_nvs.putUChar("HG", HG);
  }
  if ((!Ballon) || (Ballon > 2))  // 1:eteint 2:allumé
  {
    Serial.println("Raz Ballon : inactif pas défaut");
    Ballon = 1;
    preferences_nvs.putUChar("Bal", Ballon);
    save_modbus(160, 0);
  }
  if (HG == 2) Consigne = (float)Consigne_HG / 10;
  else Consigne = (float)Consigne_G / 10;
  if ((!MMCh) || (MMCh>2))
  {
    Serial.println("Raz Chauffage : inactif pas défaut");
    MMCh = 1;
    preferences_nvs.putUChar("MMC", MMCh);
    save_modbus(158, 0);
  }
  Serial.printf("Chauf:%i Consigne:%.1f Consigne_G:%i HG:%i Consigne_HG:%i Ballon:%i\n", MMCh, Consigne, Consigne_G, HG, Consigne_HG, Ballon);


  /*// Initialisation compteur max pour activation radiateur  si 12min
  Cpt_max_rad = preferences_nvs.getUShort("Rad_cpt", 0);  // 50 à 300
  Serial.printf("cpt_max_rad:%i\n", Cpt_max_rad);
  if ((Cpt_max_rad < 3) || (Cpt_max_rad > 300)) {
    Cpt_max_rad = 100;  // 100 : 24h si periode 12min
    Serial.printf("Raz Radiateurs: cpt_max:%i\n", Cpt_max_rad);
    preferences_nvs.putUShort("Rad_cpt", Cpt_max_rad);  //
  } else
    Serial.printf("Radiateurs: cpt_max:%i\n", Cpt_max_rad);*/

  // Initialisation ratio Tint sur loi d'eau (écart par rapport à 20°C)
  loi_eau_Tint = float(preferences_nvs.getUChar("LoiTint", 0))/100;
  if ((loi_eau_Tint<0.5) || (loi_eau_Tint > 2.5))  // autour de 1.5
  {
    loi_eau_Tint = 1.5;
    Serial.printf("Raz loi_eau_Tint : valeur par defaut:%.2f\n\r", loi_eau_Tint);
    preferences_nvs.putUChar("LoiTint", (uint8_t)(loi_eau_Tint*100));
  }
  else
    Serial.printf("loi_eau_Tint : %.2f\n\r", loi_eau_Tint);

  // Initialisation du PID
  myPID.SetMode(AUTOMATIC);                          // Active le PID
  myPID.SetOutputLimits(-min_max_pid, min_max_pid);  // Limites de la commande (-10+10)


}


// setup apres la lecture nvs, avant démarrage reseau
void setup_1()
{


  // initialisation capteur de température intérieur
  Tint = 15;
  uint8_t Tint_erreur = 0;
  #ifdef Temp_int_DHT22
    dht[0].begin();

    if (digitalRead(PIN_Tint22) == HIGH || digitalRead(PIN_Tint22) == LOW)
    {
      Tint = dht[0].readTemperature();
      Tint=10;
      if (isnan(Tint))
      {
        Tint = 20.0;
        Tint_erreur = 6;
        Serial.println("---DHT:non numérique");
      }
      else
        Serial.printf("lecture dht22 ok : %.2f\n\r", Tint);
    }
    else
      Serial.println("---DHT:signal non stable!");
  #endif

  //Serial.println(portTICK_PERIOD_MS);
  //vTaskDelay(10000 / portTICK_PERIOD_MS);
  //lect_tint();
  //vTaskDelay(1000 / portTICK_PERIOD_MS);




#ifdef Temp_int_DS18B20
  ds.begin();  // Startup librairie DS18B20
  nb_capteurs_temp = ds.getDeviceCount();
  Serial.print("Nb Capteurs DS18B20:");
  Serial.println(nb_capteurs_temp);
  if (nb_capteurs_temp > 1) nb_capteurs_temp = 1;
  int j;
  for (j = 0; j < nb_capteurs_temp; j++) {
    Serial.print(" Capteur :");
    ds.getAddress(Thermometer[j], j);
    printAddress(Thermometer[j]);
  }

  ds.requestTemperatures();  // lecture temperature interieure initiale
  for (j = 0; j < nb_capteurs_temp; j++) {
    Tint = ds.getTempCByIndex(j);
  }
#endif

  if ((Tint < 1) || (Tint > 45)) {
    Tint = 20.0;
    Tint_erreur = 7;
  }
  if (Tint_erreur) log_erreur(Code_erreur_Tint, Tint_erreur, 0);

}

// apres demarrage reseau
void setup_2()
{

}



void appli_event_on(systeme_eve_t evt)
{
}

void appli_event_off(systeme_eve_t evt)
{
}

// type 1
uint8_t requete_Get_appli(const char* var, float *valeur)
  //uint8_t requete_Get_appli (String var, float *valeur) 
{
  uint8_t res=1;

  if (strncmp(var, "Tint",5) == 0) {
    res = 0;
    *valeur = Tint;
  }
  if (strncmp(var, "Text",5) == 0) {
    res = 0;
    *valeur = Text;
  }
  if (strncmp(var, "TPAC",5) == 0) {
    res = 0;
    *valeur = TPAC;
  }
  if (strncmp(var, "Kp",3) == 0) {
    res = 0;
    *valeur = Kp;
  }
  if (strncmp(var, "Ki",3) == 0) {
    res = 0;
    *valeur = Ki;
  }
  if (strncmp(var, "Kd",3) == 0) {
    res = 0;
    *valeur = Kd;
  }
  if (strncmp(var, "consigne",9) == 0) {
    res = 0;
    *valeur = Consigne;
  }
  if (strncmp(var, "HG",3) == 0) {
    res = 0;
    *valeur = HG;
  }
  if (strncmp(var, "Ballon",7) == 0) {
    res = 0;
    *valeur = Ballon;
  }
  if (strncmp(var, "MMC",4) == 0) {
    res = 0;
    *valeur = MMCh;
  }
  if (strncmp(var, "codeR_pac",10) == 0) {
    res = 0;
    if (cpt_securite)  *valeur=1;
    else *valeur=0;
  }

  return res;
}

// type 1
uint8_t requete_Set_appli (String param, float valf) 
{
  uint8_t res=1;

  if (param == "consigne") 
  {
    if (HG == 2)  // Hors Gel
    {
      if ((valf >= 8.0) && (valf <= 16.0))  // 8°C a 16°C
      {
        Consigne_HG = round(valf * 10);
        Consigne = valf;
        preferences_nvs.putUChar("C_HG", Consigne_HG);
        res = 0;
      }
    } else if (HG == 1)  // normal
    {
      if ((valf >= 13.0) && (valf <= 22.0))  // 13°C à 22°C
      {
        Consigne_G = round(valf * 10);
        Consigne = valf;
        preferences_nvs.putUChar("Cons", Consigne_G);
        res = 0;
      }
    }
    //Serial.printf("HG:%i valf:%.1f Consigne:%.1f\n", HG, valf, Consigne);
  }
  
  if (param == "HG") 
  {
    if (valf)  // activation Hors gel
    {
      HG = 2;  // HG
      Consigne = (float)Consigne_HG / 10;
      Ballon = 1;  // ballon eteint
      save_modbus(160, 0);
      preferences_nvs.putUChar("HG", 2);
      preferences_nvs.putUChar("Bal", 1);
      res = 0;
    } else {
      HG = 1;  // mode normal
      Consigne = (float)Consigne_G / 10;
      Ballon = 2;  // ballon allumé
      save_modbus(160, 1);
      preferences_nvs.putUChar("HG", 1);
      preferences_nvs.putUChar("Bal", 2);
      res = 0;
    }
    //Serial.printf("HG:%i valf:%.1f Consigne:%.1f\n", HG, valf, Consigne);
  } 
  
  if (param == "Ballon")
  {
    res=0;
    if (valf)  // activation Ballon
    {
      Ballon = 2;  // ballon allumé
      preferences_nvs.putUChar("Bal", 2);
      save_modbus(160, 1);
    } else {
      Ballon = 1;  // ballon éteint
      preferences_nvs.putUChar("Bal", 1);
      save_modbus(160, 0);
    }
  }

  if (param == "MMC")
  {
    res=0;
    if (valf)  // activation Chauffage
    {
      MMCh = 2;  // chauffage allumé
      preferences_nvs.putUChar("MMC", 2);
      save_modbus(158, 1);
    } else {
      MMCh = 1;  // chauffage éteint
      preferences_nvs.putUChar("MMC", 1);
      save_modbus(158, 0);
    }
  }


  return res;
}

// type 2
uint8_t requete_GetReg_appli(int reg, float *valeur)
{
  uint8_t res=1;

  if (reg == 10)  // registre 10 : PID mode : 4
  {
    res = 0;
    *valeur = mode_pid;
  }
  if (reg == 11)  // registre 11 : Teau fixe
  {
    res = 0;
    *valeur = TRef;
  }
  if (reg == 12)  // registre 12 : Kp
  {
    res = 0;
    *valeur = Kp;
  }
  if (reg == 13)  // registre 13 : Ki
  {
    res = 0;
    *valeur = Ki;
  }
  if (reg == 14)  // registre 14 : Kd
  {
    res = 0;
    *valeur = Kd;
  }
  if (reg == 15)  // registre 15 : Loi d'eau Point 1
  {
    res = 0;
    *valeur = Pt1;
  }
  if (reg == 16)  // registre 16 : Loi d'eau Valeur au Point 1
  {
    res = 0;
    *valeur = Pt1Val;
  }
  if (reg == 17)  // registre 17 : Loi d'eau Point 2
  {
    res = 0;
    *valeur = Pt2;
  }
  if (reg == 18)  // registre 18 : Loi d'eau Valeur au Point 2
  {
    res = 0;
    *valeur = Pt2Val;
  }
  if (reg == 19)  // registre 19 : Loi d'eau Tint : ecart a 20°C
  {
    res = 0;
    *valeur = loi_eau_Tint;
  }
  if (reg == 21)  // registre 21 : valeur PID
  {
    res = 0;
    *valeur = Output;
  }
  if (reg == 22)  // registre 22 : min_max_pid
  {
    res = 0;
    *valeur = min_max_pid;
  }
  if (reg == 23)  // registre 23 : TPAC min
  {
    res = 0;
    *valeur = TPacMin;
  }
  if (reg == 24)  // registre 24 : TPAC max
  {
    res = 0;
    *valeur = TPacMax;
  }
  if (reg == 25)  // registre 25 : Seuil min Temp evaporation
  {
    res = 0;
    *valeur = ((float)(Seuil_T_Evapo)-300)/10;
  }


  if (reg == 40)  // registre 40 : Mode
  {
    res = 0;
    *valeur = Mode;
  }

  return res;
}

// type 2
uint8_t requete_SetReg_appli(int param, float valeurf)
{
  int16_t valeur = int16_t(valeurf);
  uint8_t res = 1;

  if (param == 10)  // registre 10 : mode_pid : 1, 2, 3
  {
    if ((valeur) && (valeur < 4)) {
      res = 0;
      mode_pid = valeur;
      preferences_nvs.putUChar("PID", mode_pid);
    }
  }

  if (param == 11)  // registre 11 : forcage Teau
  {
    res = 0;
    if ((valeurf >= 10) && (valeurf <= 30))  // 10 a 30°C
    {
      TRef = valeurf;
      preferences_nvs.putUShort("TRef", (uint16_t)(valeurf*10));  // enregistrement si reboot , pour le cas de mode_pid=fixe
    }
  }
  if (param == 12)  // registre 12 : Kp
  {
    if ((valeurf) && (valeurf < 2)) {
      res = 0;
      Kp = valeurf; //(float)valeur / 100;
      myPID.SetTunings(Kp, Ki, Kd);
      preferences_nvs.putUShort("Kp", (uint16_t)(valeurf*100));
    }
  }
  if (param == 13)  // registre 13 : Ki
  {
    res = 0;
    Ki = valeurf;
    myPID.SetTunings(Kp, Ki, Kd);
    preferences_nvs.putUShort("Ki", (uint16_t)(valeurf*10000));
  }
  if (param == 14)  // registre 14 : Kd
  {
    res = 0;
    Kd = valeurf;
    myPID.SetTunings(Kp, Ki, Kd);
    preferences_nvs.putUShort("Kd", (uint16_t)(valeurf*10000));
  }

  if (param == 15)  // registre 15 : Loi d'eau Point 1
  {
    uint8_t err_loi_eau = 0;
    int8_t Pt1_prov = valeur - 30;
    if ((Pt1_prov < -10) || (Pt1_prov > 15)) err_loi_eau = 1;        // -10°C à 15°C
    if ((Pt1_prov > Pt2) || (Pt2 - Pt1_prov < 10)) err_loi_eau = 1;  // au moins 10°C d'écart
    if (!err_loi_eau) {
      res = 0;
      Pt1 = valeur - 30;
      preferences_nvs.putUChar("Pt1", valeur);
    }
  }

  if (param == 16)  // registre 16 : Loi d'eau Valeur au Point 1
  {
    uint8_t err_loi_eau = 0;
    if ((valeur < Pt2Val) || (valeur - Pt2Val < 5)) err_loi_eau = 1;  // au moins 5° d'écart
    if (!err_loi_eau) {
      res = 0;
      Pt1Val = valeur;
      preferences_nvs.putUChar("Pt1Val", valeur);
    }
  }
  if (param == 17)  // registre 17 : Loi d'eau Point 2
  {
    uint8_t err_loi_eau = 0;
    if ((valeur < 10) || (valeur > 30)) err_loi_eau = 1;         // 10°C à 30°C
    if ((Pt1 > valeur) || (valeur - Pt1 < 10)) err_loi_eau = 1;  // au moins 10°C d'écart
    if (!err_loi_eau) {
      res = 0;
      Pt2 = valeur;
      preferences_nvs.putUChar("Pt2", valeur);
    }
  }

  if (param == 18)  // registre 18 : Loi d'eau Valeur au Point 2
  {
    uint8_t err_loi_eau = 0;
    if ((Pt1Val < valeur) || (Pt1Val - valeur < 5)) err_loi_eau = 1;  // au moins 5° d'écart
    if (!err_loi_eau) {
      res = 0;
      Pt2Val = valeur;
      preferences_nvs.putUChar("Pt2Val", valeur);
    }
  }
  if (param == 19)  // registre 19 : loi eau Tint , ecart a 20°C
    if ((valeurf >= 0.5) && (valeurf <2.5))  
    {
      res = 0;
      loi_eau_Tint = valeurf;
      preferences_nvs.putUChar("LoiTint", (uint8_t)(loi_eau_Tint*100));
    }


  if (param == 22)  // registre 22 : Min_max pid
  {
    if ((valeur >= 2) && (valeur <= 10)) {
      res = 0;
      min_max_pid = valeur;
      preferences_nvs.putUChar("MPid", min_max_pid);
    }
  }
  if (param == 23)  // registre 23 : TPACmin
  {
    if ((valeur >= 5) && (valeur <= 30) && (valeur < TPacMax)) {
      res = 0;
      TPacMin = valeur;
      preferences_nvs.putUChar("Tmin", TPacMin);
    }
  }
  if (param == 24)  // registre 24 : TPAC max
  {
    if ((valeur >= 15) && (valeur <= 40) && (TPacMin < valeur)) {
      res = 0;
      TPacMax = valeur;
      preferences_nvs.putUChar("Tmax", TPacMax);
    }
  }

  if (param == 25)  // registre 25 : Seuil Temp min Evaporation
  {
    if ((valeurf >= -20) && (valeurf <= 10)) {   // -20°C à 0°C
      res = 0;
      Seuil_T_Evapo = (uint16_t)(valeurf*10+300);  
      preferences_nvs.putUShort("TEv", Seuil_T_Evapo);
    }
  }

  if (param == 30)  // registre 30 : index
  {
    res = 0;
    index_val = valeur;
  }
  if (param == 31)  // registre 31 : graphique temp int
  {
    res = 0;
    graphique[index_val][0] = valeur;
    //Serial1.println(valeur);
    //sauve_nvs_16bytes(graphique_pression, "G_pression");   // bits_16:1=8bits  bits_16:2=16 bits
  }
  if (param == 32)  // registre 32 : graphique temp ext
  {
    res = 0;
    //preTransmission();
    graphique[index_val][1] = valeur;
  }
  if (param == 33)  // registre 33 : graphique temp pac
  {
    res = 0;
    //postTransmission();
    graphique[index_val][2] = valeur;
  }
  if (param == 40)  // registre 40 : mode
  {
    if ((valeur) && (valeur <= 3)) {
      res = 0;
      Mode = valeur;
      preferences_nvs.putUChar("Mode", Mode);
    }
  }

  return res;
}


uint8_t requete_Get_String_appli(uint8_t type, String var, char *valeur)
{
  uint8_t res=1;
  //int paramV = var.toInt();
  // valeur limité a 50 caractères
  
  return res;
}

uint8_t requete_Set_String_appli(int param, const char *texte)
{
  uint8_t res=1;
  IPAddress ip;

  return res;
}

// type5 : reception message ACTION par uart ou par page web
uint8_t requete_action_appli(const char *reg, const char *data)
{
  uint8_t res=1;

if (strcmp(reg, "Test1") == 0) 
    { 
      res=0; 
      requete_status(buffer_dmp, 0, 1);
      Serial.println(buffer_dmp);
    }

if (strcmp(reg, "Test2") == 0) 
    { 
      res=0; 
      uint8_t Tint_erreur = lecture_Tint(&Tint);
      Serial.println(Tint_erreur);
      Serial.println(Tint);
    }

  return res;
}


// mesure temperature interieure
// erreur :0:ok sinon erreur 4:10erreurs succ, 11 à 20 : x erreurs succ
uint8_t lecture_Tint(float *mesure) {
  uint8_t Tint_err = 7;
  float valeur = 20;

  #ifdef Temp_int_DHT22
    uint8_t pas_valid=0;
    uint8_t repet=8;
    while (repet)
    {
      
      // Timeout de sécurité pour éviter le plantage
      unsigned long startTime = millis();
      if (digitalRead(PIN_Tint22) == HIGH || digitalRead(PIN_Tint22) == LOW)
      {
        portDISABLE_INTERRUPTS();
        valeur = dht[0].readTemperature();
        //valeur = 10;
        portENABLE_INTERRUPTS();
        if (isnan(valeur)) pas_valid=4;  // non numérique
        else
        {
          if (valeur == -127) pas_valid = 1;
          if (valeur > 50) pas_valid = 2;
          if (valeur < -20) pas_valid = 3;
        }
        if (pas_valid)  // erreur
        {
          vTaskDelay(500 / portTICK_PERIOD_MS); // Petite pause
          Tint_err = pas_valid;
          repet--;
          Serial.printf("DHT22: Erreur de lecture %d, tentative %d\n", pas_valid, repet);
        }
        else // ok
        {
          Tint_err=0;
          if (repet !=8)  // au moins une erreur de lecture
            Tint_err = repet+10;
          repet=0;
          Serial.printf("DHT22: Lecture réussie: %.1f°C repet:%i\n", valeur, repet);
        }
      }
      else
      {
        Tint_err = 5;
        Serial.println("---DHT:signal non stable!");
      }

      
      // Vérifier le timeout
      if (millis() - startTime > DHT22_TIMEOUT_MS) { // Utiliser la constante définie
        Tint_err = 6; // Code d'erreur pour timeout
        repet = 0;
        Serial.println("DHT22: Timeout de lecture détecté");
      }

      //repet=0;  // 1 seule occurence
    }

  #endif

  #ifdef Temp_int_DS18B20
    ds.requestTemperatures();
    valeur = ds.getTempCByIndex(0);
  #endif

  /*if (valeur == -127) Tint_err = 1;
  if (valeur > 50) Tint_err = 2;
  if (valeur < -20) Tint_err = 3;*/
  *mesure = valeur;
  return Tint_err;
}

//mesure temperature exterieure
uint8_t lecture_Text(float *mesure) {
  uint8_t Text_erreur = 0;
  int16_t Val_Text = 1600;
  float valeur;

  #ifdef MODBUS
    Text_erreur = read_modbus(2, &Val_Text);  // registre 1-2-3 pour temp exterieure
    valeur = (float)Val_Text / 10;
  #else
    valeur = 18.0;

    /*#ifndef DEBUG_SANS_Sonde_Ext
        Val_Text = analogRead( PIN_Text );  // 0 à 4096
        //Serial.println(Val_Text);
      #endif*/
    // calibration
    // Text1:100(10°C) Text1Val:500
    // Text2:200(20°C) Text2Val:2000
    //valeur = ((float)(Text1Val-Val_Text)/(Text1Val-Text2Val)*(Text2-Text1) + Text1)/10;

    /*float Vmesure = ((float)Val_Text / resolutionADC) * 3.66;
      float Rntc = 15000 * Vmesure / (3.3 - Vmesure);  // Calcul de la résistance de la thermistance
      float T_kelvin = 1.0 / ((1.0 / 298.15) + (1.0 / TBeta) * log(Rntc / Therm0));    // Calcul de la température en Kelvin
      valeur = T_kelvin - 273.15;    // Conversion en °C */
    //Serial.printf("val_text:%i vmesure:%.3f rntc:%.0f T_kelvin:%.1f valeur:%.1f\n", Val_Text, Vmesure, Rntc, T_kelvin, valeur);
  #endif

  if ((valeur < -30.0) || (valeur > 60.0)) Text_erreur = 1;
  if (!Val_Text) Text_erreur = 2;

  *mesure = valeur;
  return Text_erreur;
}

// chaque 30 secondes, vérification si le compresseur a changé d'état par rapport à la mesure précédente
// Si oui, enreg temps cycle arret ou démarrage 
void event_mesure_compresseur()  // toutes les 30 secondes
{
  int16_t valeur_compr=0;
  uint8_t etat_compr_new=0;
  unsigned long millis_comp;
  uint8_t err = read_modbus(25, &valeur_compr);  // interrogation PAC
  if (valeur_compr) etat_compr_new=1;

  millis_comp = millis();
  if (etat_compr != etat_compr_new)  // changement d'état
  {
    etat_compr  = etat_compr_new;
    if (etat_compr)  // demarrage compresseur - actif : fin du cycle arret
    {
      milli_marche = millis_comp;  // demarre du cycle marche
      uint16_t duree_arret = (milli_marche - milli_arret) / 60000;  // en minutes
      for (uint8_t i = NB_Val_Graph - 1; i; i--)  // graphique des cycles Off
        graphique[i][4] = graphique[i - 1][4];
      graphique[0][4] = duree_arret / 4;  // division par 4 du temps d'arret, pour améliorer lisibilité du graphique
    }
    else  // arret du compresseur
    {
      milli_arret = millis_comp;  // demarre du cycle arret
      uint16_t duree_marche = (milli_arret - milli_marche) / 60000;  // en minutes
      for (uint8_t i = NB_Val_Graph - 1; i; i--)   
      {
        graphique[i][3] = graphique[i - 1][3];
        graphique[i][5] = graphique[i - 1][5];
      }
      graphique[0][3] = duree_marche;   // graphique des cycles On
      graphique[0][5] = T_evapo_min;   // graphique des Tem min Evapo
      T_evapo_min = 700; // 40°C
    }
  }
  else  // pas de changement d'etat
  {
    if (etat_compr)  // compresseur en marche
    {
      dernier_fct = (millis_comp - milli_marche) / 60000;  // en minutes
      valeur_compr = 0;
      err = read_modbus(6, &valeur_compr);  // interrogation Temperature evaporation
      if ((err) || (valeur_compr<-300) || (valeur_compr>400)) valeur_compr=0;
      uint16_t T_evap = (uint16_t)(valeur_compr+300);      
      if (T_evap < T_evapo_min) T_evapo_min = T_evap;  // enregistrement du minimum
      if (T_evapo_min < Seuil_T_Evapo)
      {
        // TODO : Arret de la PAC, avant qu'elle ne se mette en sécurité
        //save_modbus(158, 0);
        writeLog('P', 0, (uint8_t)(T_evapo_min>>4),(uint8_t)(T_evapo_min & 0x0F), "Secu_Eva");

        Serial.printf("Arret PAC securite : %i\n\r", T_evapo_min);
        // Diminution de la consigne : -10°C sur TPac
        TPAC = TPAC-10;
        //save_modbus(32, (uint16_t)(TPAC * 10));
      }
    }
    else  // compresseur a l'arret
    {
      heure_arret = (millis_comp - milli_arret) / 60000;  // en minutes
    }
  }
}


void event_mesure_temp()  // toutes les 15 minutes
{

  // regulation PAC
  uint8_t i;

  // mesure temperature interieure
  uint8_t Tint_erreur = lecture_Tint(&Tint);
  if (Tint_erreur) {
    log_erreur(Code_erreur_Tint, Tint_erreur,0);
    if (Tint_erreur < 10)
      Tint = 20;
  }

  // mesure température exterieure
  Text = 20;
  uint8_t Text_erreur;
  Text_erreur = lecture_Text(&Text);
  if (Text_erreur) {
    log_erreur(Code_erreur_Text, Text_erreur,0);
    Text = 20;
  }

  // Calcul de la température PAC moyenne en fonction de la loi d'eau sur la Temp ext
  T_obj = loi_deau(Text, (float)Consigne, &T_loi_eau);

  // calcul temperature eau PAC  - PID
  // en fonction de Tconsigne, Tinterieure,
  Input = Tint;

  if (mode_pid == 1)  // Normal
  {
    // limitation du PID de 10 à 30. Modifier pour mettre de 15° à 30°/40°
    float minP = (float)min_max_pid;
    if (T_obj - (float)TPacMin < minP) minP = T_obj - (float)TPacMin;  // 11.7 -10  < 5   => minP=1.7
    float maxP = (float)min_max_pid;
    if ((float)TPacMax - T_obj < maxP) maxP = (float)TPacMax - T_obj;
    myPID.SetOutputLimits(-minP, maxP);  // Limites de la commande (min_max_pid, 10, 30)
    Serial.printf("Min:%.1f  Max:%1.f \n\r", minP, maxP);

    myPID.Compute();
    // ajout de la loi d'eau dépendant de la temperature exterieure
    TPAC = Output + T_obj;   //(Text-Pt1)/(Pt2-Pt1)*(Pt2Val-Pt1Val)+Pt1Val;
  } else if (mode_pid == 2)  // Loi d'eau
  {
    TPAC = T_obj;
    Output = 0;
  } else if (mode_pid == 3)  // fixe
  {
    Output = 0;  // pas de chgt de TPAC
    TPAC = TRef;
  }

  if ((Text_erreur) && (Tint_erreur))  // Tpac = 18°C
    TPAC = 18.0;
  else if (Tint_erreur) {
    TPAC = T_obj;  // pas de régul
    if (mode_pid == 3) TPAC = TRef;
  }

  if (TPAC > TPacMax) TPAC = TPacMax;  // Temperature d'eau maximum : 30°C
  if (TPAC < TPacMin) TPAC = TPacMin;  // min:10°C

  //Serial.printf("out PID : Tpac:%.2f Output:%.2f\n", TPAC, Output);
  compteur_graph++;
  if (compteur_graph >= skip_graph)  // 1 valeur sur x
  {
    compteur_graph = 0;
    for (i = NB_Val_Graph - 1; i; i--) {
      graphique[i][0] = graphique[i - 1][0];
      graphique[i][1] = graphique[i - 1][1];
      graphique[i][2] = graphique[i - 1][2];
    }
    graphique[0][0] = round(Tint * 10);
    graphique[0][1] = round(Text * 10);

    int16_t TCha = 240;
    uint8_t err = read_modbus(30, &TCha);  // pour temp chauffage actuel
    if (err) TCha = 230;                   // 23° par defaut
    graphique[0][2] = TCha;
    //Serial.printf("Tcha:%i\r\n", TCha);
    /*if (Mode == 3)
      graphique[0][2] = radiateur_marche * 50 + 120;  // 12°C ou 17°*/

    //Serial.printf("Graphique:%i\n", graphique[0][2]);
  }

  if ((Mode == 1) || (Mode == 2)) {
    float Simul_t_ext = 30.0;
    float sortie_analo=0.0;

    #ifdef MODBUS
        save_modbus(32, (uint16_t)(TPAC * 10));
    #else
      // transformation en simulation de temperature exterieure, pour commande PAC
      // si T_PAC < 10 => arret = SText=30
      // si T_PAC = 20 =>       = SText=20
      // si T_PAC > 30 => max   = SText=-10
      // entre les 2 linéraire

      if (TPAC >= TPacMax)
        Simul_t_ext = -10;
      else if (TPAC > TPacMin)
        Simul_t_ext = (TPacMax - TPAC) * 40 / (TPacMax - TPacMin) - 10;

      // sortie analogique vers PAC
      // -10 => 1400;
      //  30 => 200:

      //sortie_analo = (  0.5 * ( exp(3950*(1/(Simul_t_ext+273)-1/298.1))) / 3.3 ) * resolutionADC;
      sortie_analo = S_analo_min - (Simul_t_ext + 10) * (S_analo_min - S_analo_max) / 40;
      if (sortie_analo > S_analo_min) sortie_analo = S_analo_min;
      if (sortie_analo < S_analo_max) sortie_analo = S_analo_max;

      //uint8_t resul = ledcWrite(PIN_PAC, sortie_analo);
      //uint16_t resul16 = ledcRead(PIN_PAC);
      //Serial.printf("pwm:%i %i\n", resul, resul16);
    #endif

    Serial.printf("PID:Consigne:%.1f Obj:%.1f Tint:%.1f PID:%.3f TPAC:%.1f Simu:%.1f sortie:%.1f Text:%.1f\n\r", Consigne, T_obj, Tint, Output, TPAC, Simul_t_ext, sortie_analo, Text);
  }


  /*  if (Mode == 2)  // radiateur et PAC
  {
    if (Tint > (Consigne - 0.5))  // arret chauffage
    {
      if (radiateur_marche)  // si allumé, on l'éteint
        radiateur_on(0);
      else  // on ne le rééteint qu'un fois par 24h
      {
        cpt_rad++;
        if (cpt_rad >= Cpt_max_rad)
          radiateur_on(0);
      }
    }
    if (Tint < (Consigne - 1.5))  // Tint < consigne-1.5 => on attend 24h puis => chauffage
    {
      cpt_rad++;
      if (cpt_rad >= Cpt_max_rad)
        radiateur_on(1);
    }
  }

  if (Mode == 3)  // radiateurs
  {
    if (Tint > (Consigne + 0.2))  // arret chauffage
    {
      if (radiateur_marche)  // si allumé, on l'éteint
        radiateur_on(0);
      else  // on ne le rééteint qu'un fois par 24h
      {
        cpt_rad++;
        if (cpt_rad >= Cpt_max_rad)
          radiateur_on(0);
      }
    }
    if (Tint < (Consigne - 0.2))  // Tint < consigne => chauffage
    {
      if (!radiateur_marche)  // si éteint, on l'allume
        radiateur_on(1);
      else  // on ne le rallume qu'un fois par 24h
      {
        cpt_rad++;
        if (cpt_rad >= Cpt_max_rad)
          radiateur_on(1);
      }
    }
  }*/
}

float loi_deau(float temp_ext, float temp_cons, float *Tloi) {
  float temp_obj;
  *Tloi = (temp_ext - Pt1) / (Pt2 - Pt1) * (Pt2Val - Pt1Val) + Pt1Val;
  // adaptation a la consigne
  temp_obj = *Tloi + (temp_cons - 20) * loi_eau_Tint;
  if (temp_obj < TPacMin) temp_obj = TPacMin;
  if (temp_obj > TPacMax) temp_obj = TPacMax;
  return temp_obj;
}

void lect_tint()
{
  //uint8_t Tint_erreur = 0;
  #ifdef Temp_int_DHT22
    //dht[0].begin();

    if (digitalRead(PIN_Tint22) == HIGH || digitalRead(PIN_Tint22) == LOW)
    {
      Tint = dht[0].readTemperature();
      //Tint=10;
      if (isnan(Tint))
      {
        Tint = 20.0;
        //Tint_erreur = 6;
        Serial.println("---DHT:non numérique");
      }
      else
        Serial.printf("lecture dht22 ok : %.2f\n\r", Tint);
    }
    else
      Serial.println("---DHT:signal non stable!");
  #endif
}

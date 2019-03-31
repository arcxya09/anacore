#include <iostream>
#include <cstdio>
#include <cstring>
#include <bitset>
#include <climits>

#include "TFile.h"
#include "TTree.h"
#include "TString.h"

using namespace std;

#define MADC_ENABLE
#define V1190_ENABLE
#define V830_ENABLE

#define N_MADCs 4
#define N_V1190 1
#define N_V1190_CH 128
#define N_MADC_CH 32
#define N_SCA_CH 32
#define PRINT_INTERVAL 10000
#define V1190_DELAY 0
#define GAMMA_ADC_GATE_DELAY 190
#define GAMMA_TDC_GATE_DELAY 190
#define GAMMA_ADC_ID 3

Int_t V1190_GEO[N_V1190] = {20};
// Int_t counter[N_V1190] = {0};
// ULong64_t TS_last[3][N_V1190] = {0};
// Int_t entry_last = 0;

ULong64_t pre_ts = 0;
ULong64_t flip_n = 0;

// V830 data masks

#define MAX_EVT_LEN 1024 * 1024 * 10 /* 128 MB */

#define MAX_MULTI 10

// struct MADC
// {
//  int nDWORD = N_MADCs;
//  unsigned int ADC[N_MADCs][N_MADC_CH];
//  unsigned long TS[N_MADCs];
// };

struct EVT_STRUCT
{
    int nDWORD;
    unsigned int DWORDS[MAX_EVT_LEN];
};

struct MADC_SINGLE
{
    Short_t ADC[N_MADC_CH];
    ULong64_t TS;
};

struct V1190_STRUCT
{
    Int_t GEO;
    UInt_t T[N_V1190_CH];
    UInt_t TMULTI[N_V1190_CH][MAX_MULTI];
    ULong64_t EVT_ID;
    ULong64_t TS;
    Int_t MULTI[N_V1190_CH];
};

void ParseFile(TString fin_name, TString fout_name);
void MADCParser(struct MADC_SINGLE **madc, struct EVT_STRUCT *evt, TTree **tout);
void V830Parser(int *sca, struct EVT_STRUCT *sca_evt);
void V1190Parser(struct V1190_STRUCT **v1190, struct EVT_STRUCT *evt, TTree **tout, int i);

int main(int argc, char const *argv[])
{

    TString fin_name, fout_name;
    char fin_path[] = "../raw", fout_path[] = "../parsed";

    if (argc == 4)
    {
        for (int runnum = atoi(argv[2]); runnum <= atoi(argv[3]); ++runnum)
        {
            fin_name = TString::Format("%s/%s%05d.root", fin_path, argv[1], runnum);
            fout_name = TString::Format("%s/%s%05d.parsed.root", fout_path, argv[1], runnum);
            printf("Parsing %s\n", fin_name.Data());
            ParseFile(fin_name, fout_name);
        }
    }
    else if (argc == 3)
    {
        fin_name = TString::Format("%s/%s%05d.root", fin_path, argv[1], atoi(argv[2]));
        fout_name = TString::Format("%s/%s%05d.parsed.root", fout_path, argv[1], atoi(argv[2]));
        ParseFile(fin_name, fout_name);
    }
    else if (argc == 2)
    {
        fin_name = TString::Format("%s/%s", fin_path, argv[1]);
        fout_name = TString::Format("%s/%s.parsed.root", fout_path, argv[1]);
        ParseFile(fin_name, fout_name);
    }
    else
    {
        printf("usage: ./parser filebase runnum \n");
    }

    return 0;
}

void ParseFile(TString fin_name, TString fout_name)
{
    pre_ts = 0;
    flip_n = 0;
    TFile *fin = new TFile(fin_name);
    if (fin->IsZombie())
    {
        printf("file open error\n");
        delete fin;
        return;
    }

    //  Parse MADC and V1190

    struct EVT_STRUCT *evt_adc = new struct EVT_STRUCT;
    struct EVT_STRUCT *evt_tdc = new struct EVT_STRUCT;

    TTree *tin = (TTree *)fin->Get("Trigger");
    if (!tin)
    {
        printf("tree get error\n");
        exit(-1);
    }

    tin->SetBranchAddress("ADC0", evt_adc);

    TFile *fout = new TFile(fout_name, "RECREATE");

    TTree **tout = new TTree *[N_MADCs];

    struct MADC_SINGLE **madc = new struct MADC_SINGLE *[N_MADCs];

    for (int i = 0; i < N_MADCs; ++i)
    {
        tout[i] = new TTree(TString::Format("adc%d", i), TString::Format("adc%d", i));
        madc[i] = new struct MADC_SINGLE;

        tout[i]->Branch("adc", madc[i]->ADC, TString::Format("adc[%d]/S", N_MADC_CH));
        tout[i]->Branch("ts", &madc[i]->TS, "ts/l");
    }

    tin->SetBranchAddress("TDC0", evt_tdc);

    struct V1190_STRUCT **v1190 = new struct V1190_STRUCT *[N_V1190];

    TTree **tout_v1190 = new TTree *[N_V1190];
    UInt_t TMULTI[128][10];
    for (int i = 0; i < N_V1190; ++i)
    {
        tout_v1190[i] = new TTree(TString::Format("tdc%d", i), TString::Format("tdc%d", i));
        v1190[i] = new struct V1190_STRUCT;

        v1190[i]->GEO = V1190_GEO[i];

        tout_v1190[i]->Branch("t", v1190[i]->T, TString::Format("t[%d]/i", N_V1190_CH));
        tout_v1190[i]->Branch("ts", &v1190[i]->TS, "ts/l");
        // tout_v1190[i]->Branch("tmulti", v1190[i]->TMULTI, TString::Format("tmulti[%d][multi]/i", N_V1190_CH));
        // tout_v1190[i]->Branch("multi", v1190[i]->MULTI, TString::Format("multi[%d]/i", N_V1190_CH));
    }

    int nentries = tin->GetEntries();
    //  int nentries = 100000;

    for (int i = 0; i < nentries; i++)
    {
        tin->GetEntry(i);
        //  if (i % 10 != 0) continue;
        MADCParser(madc, evt_adc, tout);
        //  tin->GetEntry(i + 1);
        V1190Parser(v1190, evt_tdc, tout_v1190, i);
        //      cout<<i<<endl;
        if (i % PRINT_INTERVAL == 0)
        {
            printf("%d/%d FLIP %d\r", i, nentries, flip_n);
            fflush(stdout);
        }
    }

#ifdef V830_ENABLE
    //  Parse V830

    TTree *tin_sca = (TTree *)fin->Get("Scaler");
    struct EVT_STRUCT *sca_evt = new struct EVT_STRUCT;
    tin_sca->SetBranchAddress("SCAL", sca_evt);

    nentries = tin_sca->GetEntries();

    int sca[N_SCA_CH];

    TTree *tout_sca = new TTree("sca", "sca");
    tout_sca->Branch("sca", sca, TString::Format("sca[%d]/I", N_SCA_CH));

    for (int i = 0; i < nentries; ++i)
    {
        tin_sca->GetEntry(i);
        V830Parser(sca, sca_evt);
        tout_sca->Fill();
    }

    tout_sca->Write();

#endif

    for (int i = 0; i < N_MADCs; ++i)
    {
        tout[i]->Write();
    }

    for (int i = 0; i < N_V1190; ++i)
    {
        tout_v1190[i]->Write();
    }

    // int an = tout[0]->GetEntries();
    // tout[0]->GetEntry(an);
    // int ats = madc[0]->TS;
    // int tn = tout_v1190[1]->GetEntries();
    // tout_v1190[1]->GetEntry(tn);
    // int tts = v1190[1]->TS;
    // if (abs(tts - ats) > 100000000) cout << fout_name << endl;

    fout->Close();
    fin->Close();
}

// MADC data masks
#define MADC_EVENT_SIG_MASK 0xC0000000
#define MADC_EVENT_DATA_MASK 0xFFE00000
#define MADC_EVENT_MODULE_ID_MASK 0x00FF0000
#define MADC_EVENT_CHANNEL_MASK 0x001F0000
#define MADC_EVENT_DATAZONE_MASK 0x00003FFF

#define MADC_EVENT_HEADER_SIG 0x40000000
#define MADC_EVENT_DATA_SIG 0x00000000
#define MADC_EVENT_TRAILER_SIG 0xC0000000

#define MADC_EVENT_DATA 0x04000000
#define MADC_EVENT_TS_HB 0x04800000

void MADCParser(struct MADC_SINGLE **madc, struct EVT_STRUCT *evt, TTree **tout)
{
    int module_id = 0, ch = -1;
    unsigned long ts_low, ts_high, ts;

    for (int i = 0; i < evt->nDWORD; ++i)
    {
        unsigned int data = evt->DWORDS[i];
        // cout << bitset<4>(data >> 28) << " ";
        // cout << bitset<4>(data >> 24) << " ";
        // cout << bitset<4>(data >> 20) << " ";
        // cout << bitset<4>(data >> 16) << " ";
        // cout << bitset<4>(data >> 12) << " ";
        // cout << bitset<4>(data >> 8) << " ";
        // cout << bitset<4>(data >> 4) << " ";
        // cout << bitset<4>(data >> 0) << endl;
        // getchar();
        if ((data & MADC_EVENT_SIG_MASK) == MADC_EVENT_HEADER_SIG)
        {
            ts_low = 0;
            ts_high = 0;
            ts = 0;
            module_id = 0;
            ch = -1;

            module_id = ((data & MADC_EVENT_MODULE_ID_MASK) >> 16);
            // std::cout << module_id << std::endl;
            if (module_id == 0 || module_id > N_MADCs)
            {
                module_id = 0;
                continue;
            }

            for (int j = 0; j < N_MADC_CH; ++j)
            {
                madc[module_id - 1]->ADC[j] = 0;
            }
            madc[module_id - 1]->TS = ULONG_MAX;
        }
        else if (module_id == 0)
        {
            continue;
        }
        else if ((data & MADC_EVENT_SIG_MASK) == MADC_EVENT_TRAILER_SIG)
        {

            ts_low = data & (~MADC_EVENT_SIG_MASK);

            // if (ts_low + (ts_high << 30) < ts)
            // {
            //  cout << "ts error " << endl;
            // }

            // ts = ts_low + (ts_high << 30);

            madc[module_id - 1]->TS = ts_low + (ts_high << 30) + V1190_DELAY;
            if (module_id - 1 == GAMMA_ADC_ID)
            {
                madc[module_id - 1]->TS -= GAMMA_ADC_GATE_DELAY;
            }

            tout[module_id - 1]->Fill();

            module_id = 0;
            ts = 0;
            ts_low = 0;
            ts_high = 0;
            ch = 0;
        }
        else if ((data & MADC_EVENT_SIG_MASK) == MADC_EVENT_DATA_SIG)
        {
            if ((data & MADC_EVENT_DATA_MASK) == MADC_EVENT_DATA)
            {
                ch = ((data & MADC_EVENT_CHANNEL_MASK) >> 16);
                if (ch >= N_MADC_CH && ch < 0)
                {
                    cout << "invalid channel" << endl;
                    continue;
                }
                // if (module_id == 1 && ch == 0) {
                //  if (amp > 1500) C1 += 1;
                //  if (amp > 100 && amp < 1500) C2++;
                // }
                //      printf("%-10x %-5d %-5d %-5d %-5d %-5d\n",data,module_id,ch,amp,C1,C2);
                madc[module_id - 1]->ADC[ch] = (data & MADC_EVENT_DATAZONE_MASK);
            }
            else if ((data & MADC_EVENT_DATA_MASK) == MADC_EVENT_TS_HB)
            {
                ts_high = data & 0x0000FFFF;
            }
        }
    }
}

void V830Parser(int *sca, struct EVT_STRUCT *sca_evt)
{
    // cout << endl;
    for (int i = 0; i < N_SCA_CH; ++i)
    {
        sca[i] = -1;
    }
    for (int i = 1; i < sca_evt->nDWORD - 1; ++i)
    {
        int data = sca_evt->DWORDS[i];

        // cout << bitset<4> (data >> 28) << " ";
        // cout << bitset<4> (data >> 24) << " ";
        // cout << bitset<4> (data >> 20) << " ";
        // cout << bitset<4> (data >> 16) << " ";
        // cout << bitset<4> (data >> 12) << " ";
        // cout << bitset<4> (data >> 8) << " ";
        // cout << bitset<4> (data >> 4) << " ";
        // cout << bitset<4> (data >> 0) << endl;
        // getchar();

        // int sig = (sca_evt->DWORDS[i] >> 26) & 0x1;
        // if (sig == 1)
        // {
        //     /* header */
        //     continue;
        // }
        // else if (sig == 0)
        // {
        // data
        // int ch = sca_evt->DWORDS[i] >> 27;
        // int val = sca_evt->DWORDS[i] & 0x03FFFFFF;
        // if (ch >= N_SCA_CH)
        // {
        //     printf("error channel\n");
        //     printf("i=%d --> data=%u ch=%d sca=%d\n", i, sca_evt->DWORDS[i], ch, val);
        //     exit(-1);
        // }
        int ch = i - 1;
        int val = sca_evt->DWORDS[i];

        sca[ch] = val;
        // printf("i=%d --> data=%u ch=%d sca=%d\n", i, sca_evt->DWORDS[i], ch, sca[ch]);
        // }
    }
}

#define V1190_WORD_IDENTITY_MASK 0xF8000000

#define V1190_WORD_IDENTITY_HEADER 0X40000000
#define V1190_WORD_IDENTITY_TRAILER 0X80000000
#define V1190_WORD_IDENTITY_ETTT 0X88000000
#define V1190_WORD_IDENTITY_TDC_HEADER 0X08000000
#define V1190_WORD_IDENTITY_TDC_MEAS 0X00000000
#define V1190_WORD_IDENTITY_TDC_ERROR 0X20000000
#define V1190_WORD_IDENTITY_TDC_TRAILER 0X18000000
#define V1190_WORD_IDENTITY_TDC_FILLER 0XC0000000

int ValidV1190GEO(int geo)
{
    for (int i = 0; i < N_V1190; ++i)
    {
        if (geo == V1190_GEO[i])
        {
            return i;
        }
    }
    return -1;
}

void V1190Parser(struct V1190_STRUCT **v1190, struct EVT_STRUCT *evt, TTree **tout, int ennnn)
{
    int geo = 0, ch = -1, module_id = -1, marker;
    ULong64_t ts_low, ts_high, ts;

    struct V1190_STRUCT *p_tar_v1190 = NULL;
    TTree *p_tar_tout = NULL;

    for (int i = 0; i < evt->nDWORD; ++i)
    {
        marker = 0;
        unsigned int data = evt->DWORDS[i];
        unsigned int identity = (data & V1190_WORD_IDENTITY_MASK);

        //   printf("%0x %u\n", identity, data);

        if (identity == V1190_WORD_IDENTITY_HEADER)
        {
            ts_low = 0;
            ts_high = 0;
            ts = 0;
            geo = 0;
            ch = -1;

            geo = (data & 0x1F);
            // printf("Header %d\n", geo);
            module_id = ValidV1190GEO(geo);
            if (module_id == -1)
            {
                geo = 0;
                continue;
            }
            p_tar_v1190 = v1190[module_id];
            p_tar_tout = tout[module_id];

            // printf("%d\n",(data>>5)&0x3FFFFF);
            p_tar_v1190->EVT_ID = (data >> 5) & 0x3FFFFF;

            for (int j = 0; j < N_V1190_CH; ++j)
            {
                p_tar_v1190->T[j] = 0;
                p_tar_v1190->MULTI[j] = 0;
                for (int k = 0; k < MAX_MULTI; ++k)
                {
                    p_tar_v1190->TMULTI[j][k] = 0;
                }
            }
            p_tar_v1190->TS = ULONG_MAX;
        }
        else if (module_id == -1 || p_tar_v1190 == NULL || p_tar_tout == NULL)
        {
            continue;
        }
        else if (identity == V1190_WORD_IDENTITY_TDC_MEAS)
        {
            ch = (data >> 19) & 0x7F;
            if (ch > N_V1190_CH - 1 && ch < 0)
            {
                cout << "invalid channel" << endl;
                continue;
            }
            // if (module_id == 1 && ch == 0) {
            //  if (amp > 1500) C1 += 1;
            //  if (amp > 100 && amp < 1500) C2++;
            // }
            //      printf("%-10x %-5d %-5d %-5d %-5d %-5d\n",data,module_id,ch,amp,C1,C2);

            UInt_t tmp = (data & 0x7FFFF);
            if (p_tar_v1190->MULTI[ch] < MAX_MULTI)
            {
                p_tar_v1190->TMULTI[ch][p_tar_v1190->MULTI[ch]] = tmp;
                p_tar_v1190->MULTI[ch]++;
                // if(p_tar_v1190->MULTI[ch]++>1)printf("Multi-hit overflow!\n");
            }
            else
            {
                // printf("Multi-hit overflow!\n");
            }

            if (p_tar_v1190->T[ch] == 0)
            {
                p_tar_v1190->T[ch] = tmp;
            }
            // printf("Data %d %d\n", ch, (data & 0x7FFFF));
        }
        else if (identity == V1190_WORD_IDENTITY_TRAILER)
        {
            ts_low = data & 0x1F;
            // printf("Trailer %d\n", p_tar_v1190);

            // if (ts_low + (ts_high << 30) < ts)
            // {
            //  cout << "ts error " << endl;
            // }

            // ts = ts_low + (ts_high << 30);

            ULong64_t ts_temp;
            ts_temp = ts_low + (ts_high << 5);
            // printf("EVT: %d TS: %u", p_tar_v1190->EVT_ID, ts_temp);
            // getchar();
            if (pre_ts > ts_temp && pre_ts - ts_temp > 0x0FFFFFF)
            {
                flip_n++;
                // printf("EVT %d Flip %d\n", p_tar_v1190->EVT_ID, flip_n);
            }
            pre_ts = ts_temp;
            // //          if(ts_temp < TS_last[module_id] && (ennnn-entry_last > 10)) {
            //             if ((TS_last[0][module_id] < TS_last[1][module_id]) && (TS_last[1][module_id] > TS_last[2][module_id]) && (TS_last[2][module_id] < ts_temp) && (TS_last[1][module_id] - TS_last[0][module_id] < 100000000) && (TS_last[1][module_id] - TS_last[2][module_id] > 4000000000) && (ts_temp - TS_last[2][module_id] < 100000000))
            //             {
            //                 counter[module_id]++;
            // //              entry_last = ennnn;
            //                 cout << module_id << " " << TS_last[0][module_id] << " " << TS_last[1][module_id] << " " << TS_last[2][module_id] << " " << ts_temp << " " << endl;
            //             }
            //             p_tar_v1190->TS = TS_last[2][module_id] + (ULong64_t)counter[module_id] * (ULong64_t)0x100000000;

            //             TS_last[0][module_id] = TS_last[1][module_id];
            //             TS_last[1][module_id] = TS_last[2][module_id];
            //             TS_last[2][module_id] = ts_temp;

            //          if(marker== 0)p_tar_v1190->TS = TS_last[module_id];
            //                       if(marker==-1)p_tar_v1190->TS=0;
            //    p_tar_v1190->TS = (ts_temp + flip_n * (0xFFFFFFFF + 1) - GAMMA_TDC_GATE_DELAY);
            p_tar_v1190->TS = (ts_temp + flip_n * 0x100000000 - GAMMA_TDC_GATE_DELAY);
            // printf("ts_temp=%u  TS=%u\n", ts_temp, p_tar_v1190->TS);
            p_tar_tout->Fill();

            module_id = -1;
            geo = 0;
            ts = 0;
            ts_low = 0;
            ts_high = 0;
            ch = 0;
        }
        else if (identity == V1190_WORD_IDENTITY_ETTT)
        {
            ts_high = data & 0x7FFFFFF;
            // printf("ETTT %d\n", ts_high);
        }
        else if (identity == V1190_WORD_IDENTITY_TDC_HEADER)
        {
        }
        else if (identity == V1190_WORD_IDENTITY_TDC_TRAILER)
        {
        }
        else if (identity == V1190_WORD_IDENTITY_TDC_ERROR)
        {
            // printf("%d: %x\n",(data>>24)&0x03,data&0xFF);
        }
        else if (identity == V1190_WORD_IDENTITY_TDC_FILLER)
        {
        }
        else
        {
            printf("Invalid block %08x\n", data);
        }
    }
}

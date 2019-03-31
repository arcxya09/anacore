#include <iostream>
#include <climits>
#include <time.h>
#include "TFile.h"
#include "TTree.h"
#include "TString.h"

/*////
Helper
This code is to align the ADC and TDC events in the parsed data.

- "TS" or "ts" --> TimeStamp
- "SINGLE" --> single event

///*/

using namespace std;

#define Abs(x) ((x) >= 0 ? (x) : (-(x)))

#define N_MADCs 4
#define N_MADC_CH 32

#define N_V1190 1
#define N_V1190_CH 128

#define N_SCA_CH 32
#define PRINT_INTERVAL 10000

#define ALIGN_LIMIT 10

int cnt;

short ADC[N_MADCs][N_MADC_CH];

TString filename;

struct EVENT
{
    Short_t ADC[N_MADCs][N_MADC_CH];
    UInt_t TDC[N_V1190][N_V1190_CH];
    ULong64_t TS;
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
    ULong64_t TS;
};

void Align(TTree *tout, struct EVENT *evt_aligned, TTree **tmadc, TTree **tv1190, int *nmadc, int *nv1190);
void AlignFile(TString fin_name, TString fout_name);
int Minimal(unsigned long *array, int length);
int Maximum(int *array, int length);

int main(int argc, char const *argv[])
{
    if (argc == 3)
    {
        char fin_path[] = "../parsed", fout_path[] = "../aligned";
        filename = TString::Format("%s%05d.root", argv[1], atoi(argv[2]));
        TString fin_name = TString::Format("%s/%s%05d.parsed.root", fin_path, argv[1], atoi(argv[2]));
        TString fout_name = TString::Format("%s/%s%05d.aligned.root", fout_path, argv[1], atoi(argv[2]));

        AlignFile(fin_name, fout_name);
    }
    else if (argc == 4)
    {

        for (int runnum = atoi(argv[2]); runnum <= atoi(argv[3]); ++runnum)
        {
            char fin_path[] = "../parsed", fout_path[] = "../aligned";
            filename = TString::Format("%s%05d.root", argv[1], runnum);
            TString fin_name = TString::Format("%s/%s%05d.parsed.root", fin_path, argv[1], runnum);
            TString fout_name = TString::Format("%s/%s%05d.aligned.root", fout_path, argv[1], runnum);

            AlignFile(fin_name, fout_name);
        }
    }
    else
    {
        printf("usage: \n      ./align filebase runnum \n   or ./align filebase start stop\n");
    }

    return 0;
}

void AlignFile(TString fin_name, TString fout_name)
{

    TFile *fin = new TFile(fin_name);
    if (fin->IsZombie())
    {
        printf("file open error\n");
        delete fin;
        return;
    }

    struct EVENT *evt_aligned = new struct EVENT;

    TFile *fout = new TFile(fout_name, "RECREATE");

    TTree *tout = new TTree("tree", "tree");

    tout->Branch("adc", evt_aligned->ADC, TString::Format("adc[%d][%d]/S", N_MADCs, N_MADC_CH));
    tout->Branch("tdc", evt_aligned->TDC, TString::Format("tdc[%d][%d]/i", N_V1190, N_V1190_CH));
    tout->Branch("ts", &evt_aligned->TS, TString::Format("ts/l"));

    // cout << "Extracting..." << endl;

    //    struct MADC_SINGLE *madc_parsed[N_MADCs];
    //   struct MADC_SINGLE **madc[N_MADCs];

    TTree *tmadc[N_MADCs];

    int nmadc[N_MADCs];

    for (int k = 0; k < N_MADCs; ++k)
    {
        char name[100];
        sprintf(name, "adc%d", k);
        // cout << name << endl;
        tmadc[k] = (TTree *)fin->Get(name);
        // madc_parsed[k] = new struct MADC_SINGLE;
        // tin[k]->SetBranchAddress("adc", madc_parsed[k]->ADC);
        // tin[k]->SetBranchAddress("ts", &madc_parsed[k]->TS);
        nmadc[k] = tmadc[k]->GetEntries();

        // madc[k] = new struct MADC_SINGLE*[nmadc[k]];

        // for (int i = 0; i < nmadc[k]; ++i)
        // {
        //     madc[k][i] = new struct MADC_SINGLE;

        //     tin[k]->GetEntry(i);

        //     for (int ch = 0; ch < N_MADC_CH; ++ch)
        //     {
        //         madc[k][i]->ADC[ch] = madc_parsed[k]->ADC[ch];
        //     }
        //     madc[k][i]->TS = madc_parsed[k]->TS;
        // }
    }

    TTree *tv1190[N_V1190];
    // struct V1190_STRUCT **v1190[N_V1190];
    // struct V1190_STRUCT *v1190_parsed[N_V1190];

    int nv1190[N_V1190];
    for (int k = 0; k < N_V1190; ++k)
    {
        char name[100];
        sprintf(name, "tdc%d", k);
        // cout << name << endl;
        tv1190[k] = (TTree *)fin->Get(name);
        // v1190_parsed[k] = new struct V1190_STRUCT;
        // tin_v1190[k]->SetBranchAddress("t", v1190_parsed[k]->T);
        // tin_v1190[k]->SetBranchAddress("ts", &v1190_parsed[k]->TS);

        nv1190[k] = tv1190[k]->GetEntries();

        // v1190[k] = new struct V1190_STRUCT*[nv1190[k]];
        // for (int i = 0; i < nv1190[k]; ++i)
        // {
        //     v1190[k][i] = new struct V1190_STRUCT;

        //     tin_v1190[k]->GetEntry(i);

        //     for (int ch = 0; ch < N_V1190_CH; ++ch)
        //     {
        //         v1190[k][i]->T[ch] = v1190_parsed[k]->T[ch];
        //         if (v1190[k][i]->T[ch] != 0) {
        //             //cout<<v1190[k][i]->T[ch]<<endl;
        //             //exit(1);
        //         }
        //     }
        //     v1190[k][i]->TS = v1190_parsed[k]->TS;
        // }
    }

    // cout << "Aligning..." << endl;
    Align(tout, evt_aligned, tmadc, tv1190, nmadc, nv1190);

    tout->Write();

    fout->Close();

    cout << endl;

    fin->Close();
}

void Align(TTree *tout, struct EVENT *evt_aligned, TTree **tmadc, TTree **tv1190, int *nmadc, int *nv1190)
{
    int search_height[N_MADCs + N_V1190];
    for (int i = 0; i < N_MADCs + N_V1190; ++i)
    {
        search_height[i] = 1;
    }
    int search_sig = 0;
    int sr = Maximum(nmadc, N_MADCs);

    struct MADC_SINGLE *madc_parsed[N_MADCs];

    for (int k = 0; k < N_MADCs; ++k)
    {
        madc_parsed[k] = new struct MADC_SINGLE;
        tmadc[k]->SetBranchAddress("adc", madc_parsed[k]->ADC);
        tmadc[k]->SetBranchAddress("ts", &madc_parsed[k]->TS);
    }

    struct V1190_STRUCT *v1190_parsed[N_V1190];

    for (int k = 0; k < N_V1190; ++k)
    {
        v1190_parsed[k] = new struct V1190_STRUCT;
        tv1190[k]->SetBranchAddress("t", v1190_parsed[k]->T);
        tv1190[k]->SetBranchAddress("ts", &v1190_parsed[k]->TS);
    }

    while (search_sig < N_MADCs + N_V1190)
    {
        unsigned long ts[N_MADCs + N_V1190];
        for (int i = 0; i < N_MADCs; ++i)
        {
            ts[i] = ULONG_MAX;
            for (int j = 0; j < N_MADC_CH; ++j)
            {
                evt_aligned->ADC[i][j] = 0;
            }
            if (search_height[i] >= nmadc[i])
            {
                continue;
            }
            tmadc[i]->GetEntry(search_height[i]);
            while (madc_parsed[i]->TS == ULONG_MAX)
            {
                search_height[i] += 1;
                tmadc[i]->GetEntry(search_height[i]);
                printf("ADC %d invalid time stamp\n", i);
                if (search_height[i] >= nmadc[i])
                {
                    break;
                }
            }
            ts[i] = madc_parsed[i]->TS;
        }

        for (int i = 0; i < N_V1190; ++i)
        {
            ts[N_MADCs + i] = ULONG_MAX;
            for (int j = 0; j < N_V1190_CH; ++j)
            {
                evt_aligned->TDC[i][j] = 0;
            }

            tv1190[i]->GetEntry(search_height[N_MADCs + i]);

            if (search_height[N_MADCs + i] >= nv1190[i])
            {
                continue;
            }

            while (v1190_parsed[i]->TS == ULONG_MAX)
            {
                search_height[N_MADCs + i] += 1;
                tv1190[i]->GetEntry(search_height[N_MADCs + i]);
                printf("V1190 %d invalid time stamp\n", i);
                if (search_height[N_MADCs + i] >= nv1190[i])
                {
                    break;
                }
            }
            ts[N_MADCs + i] = v1190_parsed[i]->TS;
        }

        evt_aligned->TS = ULONG_MAX;
        int index = Minimal(ts, N_MADCs + N_V1190);

        if (index == -1)
        {
            break;
        }
        bool adc_found = false;
        evt_aligned->TS = ts[index];

        for (int i = 0; i < N_MADCs; ++i)
        {
            if (search_height[i] >= nmadc[i])
            {
                continue;
            }
            if (Abs(madc_parsed[i]->TS - ts[index]) < ALIGN_LIMIT)
            {
                // evt_aligned->TS[i] =  madc_parsed[i][search_height[i]]->TS;
                adc_found = true;
                for (int j = 0; j < N_MADC_CH; ++j)
                {
                    evt_aligned->ADC[i][j] = madc_parsed[i]->ADC[j];
                }
                search_height[i]++;
            }
        }

        for (int i = 0; i < N_V1190; ++i)
        {
            if (search_height[N_MADCs + i] >= nv1190[i])
            {
                continue;
            }

            if (Abs(v1190_parsed[i]->TS - ts[index]) < ALIGN_LIMIT)
            {

                // evt_aligned->TS[i] =  madc_parsed[i][search_height[i]]->TS;
                for (int j = 0; j < N_V1190_CH; ++j)
                {
                    evt_aligned->TDC[i][j] = v1190_parsed[i]->T[j];
                    //                   cout << " i= "<<i<< " j= "<<j<< endl;
                    if (i == 0 && j == 64)
                    {
                        //cout << v1190_parsed[i][search_height[N_MADCs + i]]->T[j] << endl;
                        //exit(1);
                    }
                }
                search_height[N_MADCs + i]++;
            }
        }

        if (adc_found == true)
        {
            tout->Fill();
        }

        search_sig = 0;
        for (int i = 0; i < N_MADCs; ++i)
        {
            if (search_height[i] >= nmadc[i])
            {
                search_sig += 1;
            }
        }
        for (int i = 0; i < N_V1190; ++i)
        {
            if (search_height[N_MADCs + i] >= nv1190[i])
            {
                search_sig += 1;
            }
        }

        if (search_height[sr] % PRINT_INTERVAL == 0)
        {
            printf("%s %d/%d\r", filename.Data(), search_height[sr], nmadc[sr]);
            fflush(stdout);
        }

        /*
        for (int i=0; i< N_MADCs; ++i)
        {
            printf("%d ", nmadc[i]);
        }
        for (int i=0; i< N_V1190; ++i)
        {
            printf("%d ", nv1190[i]);
        }
        cout<<endl;
        for (int i=0; i< N_MADCs+N_V1190; ++i)
        {
            printf("%d ", search_height[i]);
        }
        cout<<endl;
        for (int i=0; i< N_MADCs+N_V1190; ++i)
        {
            printf("%d ", ts[i]);
        }
        cout<<endl;
        cout<<search_sig<<endl;
        cout<<index<<endl;
        exit(0);
        */
    }
}

int Minimal(unsigned long *array, int length)
{
    unsigned long min = ULONG_MAX - 1;
    int index = -1;

    for (int i = 0; i < length; ++i)
    {
        if (array[i] == 0)
        {
            array[i] = ULONG_MAX;
        }
        if (array[i] == ULONG_MAX)
        {
            continue;
        }
        if (min > array[i])
        {
            index = i;
            min = array[i];
        }
    }
    return index;
}

int Maximum(int *array, int length)
{
    int max = 0;
    int index = -1;

    for (int i = 0; i < length; ++i)
    {
        if (array[i] > max)
        {
            max = array[i];
            index = i;
        }
    }
    return index;
}

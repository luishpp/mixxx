# Análise de mixes de referência

Ferramenta e achados de estudar sets reais para informar o motor `music_sync`.
Ela **mede** o áudio (FFT sobre as amostras) — **não escuta**. Um set é um dado, não uma lei.

```bash
pip install -r requirements.txt
python analyze_mix.py "caminho/para/mix.mp3"
```

Os MP3 de referência ficam **fora do repositório** (são música comercial — regra do `CLAUDE.md`).

## Limites honestos

- Energia sozinha **não separa** troca de faixa de breakdown interno.
- Não lê as **transições harmônicas** (precisaria de tom por trecho).
- Não nomeia o **tipo** de transição (crossfade vs EQ vs cut).

## Sets analisados (2026-07-13)

Dois sets da mesma linhagem (Vintage Culture / KVSH / Fancy Inc — house/brazilian bass):

| | 2020 (Dirty/Prydz) | 2021 (Alok/Bruno Be) |
|---|---:|---:|
| Duração | 63,7 min | 126,4 min |
| Bitrate | 239 kbps | 128 kbps |
| **Tempo global** | **123,0 BPM** | **123,0 BPM** |
| Desvio de tempo (8 trechos) | **0,0** | **0,0** |
| Grave despenca nas quedas | 30/33 (91%) | 78/81 (96%) |
| Queda mediana | 9 s | 10 s |
| Intervalo entre quedas | 1,9 min | 1,6 min |
| Posição do pico | ~38% | ~57% |

## O que se confirma (dois sets independentes)

1. **Tempo travado.** Ambos a **123,0 BPM, desvio 0,0** em todos os trechos. Não é acaso — é o
   traço definidor do gênero. **Valida** nosso `sync + quantize + beatsync_phase`; sugere 123–125
   como tempo-casa.
2. **Bass management é o mecanismo central.** ~90–96% das quedas coincidem com o grave saindo.
   **Valida** o Bass Swap / EQ Blend como comportamento padrão em pares dançantes e harmônicos.
3. **Quedas curtas e frequentes** (~9–10 s, a cada ~1,5–2 min).

## O que diverge — e o achado mais útil

**Nenhum dos dois sets é _late-peak_.** O pico cai em **~38%** (2020) e **~57%** (2021) — nunca perto
do fim. Nosso preset **padrão `Late peak` mira o pico em 90%** e **não descreve este gênero**.

Mais fundo: o set de 2020 é **quase plano** em energia (0,40–0,53 do começo ao fim). Isso indica que
estes DJs mantêm um **platô de energia de pista** e fazem a jornada pelo **conteúdo** (nostalgia,
vocais, grooves), não pela intensidade.

**Implicação para o motor:** nosso *energy fit* assume que a jornada **é** a curva de energia. Para
este gênero, a energia é um platô e a narrativa vive noutro lugar — o que explica por que o *energy
fit* do set real do usuário ficou baixo e por que o preset de energia parece uma alavanca fraca.

> Contexto importante: o **plano do usuário é deliberadamente late-peak** (jornada nostálgica, §6/§17:
> pico no melodic techno/trance perto do fim, retorno emocional). Portanto isto **não** sobrepõe o
> plano — é informação para escolher o preset com consciência, e uma pista de que *energy fit* talvez
> deva pesar menno para material de platô.

## Hipóteses de refinamento (a testar, não a afirmar)

- **Ancorar a janela de saída no início de um breakdown**, não na região de energia mais estável
  (hoje `computeTransitionWindows` ranqueia por estabilidade — o oposto de um breakdown). Os sets
  reais transicionam nas quedas.
- **Reduzir o peso de `energy fit`** (ou torná-lo opcional) para bibliotecas de energia plana.

# Guia do painel Music Sync DJ

Como usar o módulo `mixxx::music_sync` na prática: o que cada controle faz, o que ele lê,
o que ele grava e em que ordem usar. Descreve o comportamento **real** do código (Fase 7).

Abrir: **Opções → Music Sync**.

---

## TL;DR — a ordem que funciona

Numa biblioteca nova, use nesta ordem:

0. Marque ☑ **Enable Music Sync** (sem isso as ações ficam cinza).
1. **Analyze missing (Mixxx)** — só se houver faixas sem BPM/tom. Espere terminar (ele já re-snapshota sozinho).
2. **Read native analysis from library** — calcula tudo e grava no sidecar.
3. Escolha o **preset de energia** (combo).
4. **Generate sequence**.
5. No diálogo: escolha um par → **Preview on decks** para ouvir; ajuste **Transition/Bars** se
   quiser; **Run set** para tocar o set (ou só um ato).

> **Depois de atualizar o módulo (novo build), clique em "Read native analysis from library".**
> As janelas/seções ficam **gravadas** no sidecar — "Reload snapshots" só relê o que está lá,
> com as regras antigas. Só o *Read native analysis* recalcula.

---

## Pré-requisitos (senão nada toca)

- **O interruptor marcado**: ☑ *Enable Music Sync* — desmarcado, os botões ficam cinza.
- **Saída de áudio configurada**: *Preferences → Sound Hardware*. Evite dispositivos virtuais
  (ex.: FxSound); aponte para a placa física. Com **WASAPI**, a *Taxa de Amostragem* precisa ser
  **igual à do dispositivo no Windows** (geralmente 48000 Hz), senão dá `Invalid sample rate`.
- **Skin com ao menos 2 decks** (o preview usa deck 1 = A e deck 2 = B).
- **Faixas analisadas** (BPM/beatgrid/tom **e waveform**). Sem waveform não há energia — veja
  *Problemas comuns*.

---

## Os controles, um a um

### ☑ Enable Music Sync (stored in the sidecar)

**É o interruptor do painel.** Desmarcado, **todas as ações ficam desabilitadas** (os 4 botões e o
combo de preset) e o painel avisa *"Music Sync is off"*. O estado é gravado no sidecar
(`module_enabled`), então **persiste entre sessões** — se abrir o painel e tudo estiver cinza,
provavelmente é só marcar a caixa.

Uma ação só fica disponível quando as **três** condições valem: o **sidecar abriu**, o módulo está
**marcado** e **não há análise em andamento**. Durante o *Analyze missing* tudo (inclusive o
interruptor) fica bloqueado até terminar.

Se o sidecar falhar ao abrir, o rótulo de status avisa e tudo fica cinza — **sem afetar a
reprodução do Mixxx**.

### ▸ Read native analysis from library

**O botão que calcula tudo.** Para até **500** faixas da biblioteca (não excluídas, por ordem de id):

- carrega o **waveform-summary já armazenado** pelo Mixxx (não re-decodifica áudio);
- lê a análise nativa: **BPM, tom → Camelot, beatgrid, ReplayGain, intro/outro, duração**;
- calcula o avançado: **curva de energia, graves, frases, seções e janelas de entrada/saída**;
- **grava (upsert) no sidecar** e repinta a tabela.

Ele **não** roda o analisador do Mixxx — só lê o que o Mixxx já produziu. Se a faixa nunca foi
analisada, ela aparece com `Analyzed = No` e sem dados.

**Use quando:** primeira vez, depois de adicionar faixas, depois do *Analyze missing*, e
**sempre que atualizar o módulo** (para recalcular com as regras novas).

### ▸ Reload snapshots

**Somente leitura.** Relê as linhas já gravadas no sidecar e repinta a tabela. **Não recalcula
nada** — não toca na biblioteca, não refaz janelas nem seções.

**Use quando:** só quiser atualizar a visão do que está armazenado.

### ▸ Clear snapshots

Apaga **todos** os snapshots do sidecar (pede confirmação). **Nada se perde de forma
permanente**: tudo é recomputável com *Read native analysis from library*. Sua biblioteca,
waveforms, beatgrids e cues do Mixxx ficam intactos — são de outro banco.

**Use quando:** quiser um reset limpo (ex.: refez a biblioteca do zero e quer garantir que
nenhum resíduo sobrou).

### ▸ Analyze missing (Mixxx)

Varre até 500 faixas e seleciona as que **não têm beatgrid/BPM ou não têm tom**. Nelas roda o
**analisador nativo do Mixxx** (`WithBeats | WithWaveform`, prioridade baixa), em background:

- mostra progresso (`Analyzing i/N…`) e **desabilita os 4 botões** enquanto roda;
- ao terminar, **re-snapshota automaticamente** e repinta a tabela;
- se não houver nada pendente: *"Nothing to analyze"*.

> **Limitação conhecida:** ele só pesca faixas sem **beats/tom**. Uma faixa que **tem** beats e tom
> mas **não tem waveform** não é repescada — e sem waveform não há energia. Nesse caso, analise-a
> pela própria biblioteca do Mixxx (clique direito → *Analisar*).

### ▸ Preset de energia (combo)

*Ascending · Center peak · **Late peak** (padrão) · Waves · Constant* — a forma da jornada de
energia do set. **É lido apenas no instante em que você clica em Generate sequence.** Mudar o
combo sozinho não faz nada.

### ▸ Generate sequence

Precisa de **pelo menos 2 faixas analisadas** no sidecar (senão avisa). Roda o otimizador
(guloso + 2-opt, determinístico), produz **até 3 alternativas**, mostra a melhor e abre o
diálogo de resultado.

**Curadoria híbrida — o ato manda, a âncora fica.** Se as faixas têm o ato no comentário
(`ATO 5 | ÂNCORA | ...`):

1. **A ordem do plano sempre lidera.** A alternativa 1 é a ordem da numeração das faixas; as
   propostas do motor vêm depois, com os scores lado a lado, para você comparar. **O motor
   informa, não decide.**

   > Por quê: tentamos deixar o motor assumir quando superasse o score do plano, e o resultado
   > foi pior. O score mede exatamente aquilo que o plano **não** otimiza — o Ato 1 do set real é
   > `3B → 9B → 6B → 10A → 5A`, ruim pela roda Camelot, então o motor "ganhava" e entregava um set
   > musicalmente pior. O §8 curou por narrativa e ouvido, e os tons ainda vêm da detecção do
   > Mixxx, que erra. Comparar por score era o instrumento errado.
2. **A ordem segue a narrativa**: o motor nunca troca faixas entre atos. Faixas sem ato
   (`EXTRA`) vão para o fim.
3. **As âncoras ficam presas** na posição que o plano deu a elas. Assim *Space Explorers* fecha o
   Ato 1 e *Under Control* fecha o Ato 4. Aparecem com `[locked]` no relatório.
4. **Cada ato é julgado na sua fatia da curva de energia** — o Ato 5 é medido contra os ~75% da
   jornada onde ele realmente está, não contra a curva inteira.
5. **O peso da harmonia varia por ato** (§10.5): mais alto no Portal, Melodic House, Melodic
   Techno e Final; **mais frouxo** nos flashes nostálgicos e no peak crossover, onde o plano
   aceita tom contrastante em troca de impacto.

> Se a sua biblioteca não tem o comentário de ato, nada muda: cai na otimização global de sempre.
> As âncoras só são presas quando **todas** as faixas do ato têm número — sem isso, prender seria
> chute. Para ganhar tudo isso, prepare os arquivos com o número e o comentário do §3 do plano.

---

## A tabela

| Coluna | Significado |
|---|---|
| BPM / Camelot / Key / Duration / ReplayGain | Análise nativa do Mixxx |
| **Analyzed** | `No` = faixa sem BPM/tom (rode *Analyze missing*) |
| **Energy** | **Percentil relativo à sua biblioteca (0..1)**, não volume absoluto: 1.00 = a mais forte que você tem |
| **Phrases** | Nº de frases (16 compassos) derivadas do beatgrid |
| **Sections** | Nº de seções (Intro/Groove/Build/Drop/Breakdown/Outro) |
| **Exit @** | Início da melhor **janela de saída** (mm:ss) — onde a transição começa |

> **Por que Energy é relativo?** O valor cru é escalado pelo máximo teórico do waveform (três
> bandas saturadas ao mesmo tempo), que música real nunca atinge — a biblioteca inteira ficaria
> espremida em ~0.15–0.35, e nenhum limiar absoluto faria sentido. A jornada de energia de um set
> é comparativa por natureza, então o módulo ranqueia. O sidecar guarda o valor **cru**; a
> normalização é derivada e aplicada na leitura.

---

## O diálogo "Generated sequence"

Cabeçalho: `Best of N alternative(s) — average compatibility X%, energy fit Y%`.

Por faixa: posição, artista - título, `(BPM, Camelot)`, `[locked]` se travada.
Por par consecutivo:

```
[87%] Strong match: 1.0 BPM difference, 10B -> 10B, stable energy.
  ↳ EQ Blend — 32 bars, 88% conf
```

- **`[87%]`** = PairScore (harmonia, tempo, frase, energia, janela) + explicação.
- **`↳`** = a transição escolhida: **tipo — duração em compassos — confiança**.

### Tipos de transição e quando cada um sai

| Tipo | Critério |
|---|---|
| **Auto DJ fallback** | faixa não analisada **ou sem janelas** → não dá para mixar de verdade |
| **Breakdown swap** | tom incompatível **ou** salto de tempo — **e** a faixa que sai tem um `Breakdown`/`Outro` nos últimos 40% para pousar |
| **Cut on phrase** | o mesmo caso, mas **sem** zona de pouso: não há o que esconder o choque |
| **Bass Swap** | par dançante + harmônico + janelas confiáveis |
| **EQ Blend** | janelas confiáveis |
| **Crossfade** | o resto (modo seguro) |

> **Por que Breakdown swap antes de Cut?** O §16 nunca pede corte para tom incompatível — pede
> *"troca por breakdown"*: trazer a faixa nova atravessando um trecho de baixa energia, onde quase
> não há conteúdo tonal para brigar. Cortes secos no Portal soam mal; foi assim que descobrimos.

> **Tudo saindo "Auto DJ fallback"** = suas faixas estão sem **Exit @** (sem janelas) → sem energia
> → sem waveform. Veja *Problemas comuns*.

**Beat sync não segue o tipo, segue o plano.** Quem sabe se os tempos casam é o planejador: um
*Breakdown swap* por choque de **tom** sincroniza (os tempos batem); o mesmo tipo por salto de
**tempo** não sincroniza — cada faixa mantém o tempo dela. `Cut` e `Auto DJ fallback` nunca
sincronizam.

### Editar uma transição (RF-010)

Ao lado do seletor de par:

```
Transition: [Automatic ▾]      Bars: [Automatic ▾]
```

- **Automatic** (padrão) — o motor escolhe e explica, como sempre.
- Escolha um **tipo** e/ou um **número de compassos** (8/16/32/64) para **aquele par**. Os dois são
  independentes: *"Bass Swap, você escolhe o tamanho"* e *"como quiser, mas 64 compassos"* são
  respostas válidas.

**A sua escolha de duração vence as heurísticas.** O limite da janela e o teto de 8 compassos do
Cut são *gosto do planejador* — você acaba de passar por cima. **Só a física ainda discute**: se a
transição não couber antes da faixa acabar, ela é aparada e o relatório **avisa**. Sair da janela
também é permitido, com aviso. Nada acontece calado.

> **A escolha fica colada no _par de faixas_, não na posição** (sidecar, migration v7). Rodar
> *Generate sequence* de novo reordena posições — um ajuste preso à "posição 7" reapareceria em
> cima de **outro par**. Por isso *Automatic* é a **ausência** de registro: não decidir não é uma
> decisão.

**Preview on decks**, **Run set** e o relatório planejam pelos **mesmos** overrides — o que você
ouve na prévia é o que o set vai fazer. Ajuste → ouça → repita.

> Não existe (ainda): timeline arrastável, desenho de curva à mão, stems e presets combináveis.
> O §27 do plano põe isso na **Fase 10 (pós-MVP)**.

### Preview em dois decks

`[seletor de par]` **Preview on decks** · **Repeat** · **Cancel preview** + rótulo de estado.

### ▸ Run set (e ensaio por ato)

Toca a sequência inteira nos decks, entregando de uma faixa para a outra sozinho. O seletor ao
lado escolhe **o quanto** rodar:

| Escolha | Para quê |
|---|---|
| **Whole set** | o set completo, do Ato 1 ao 7 |
| **Act N** | ensaiar um ato isolado |
| **Act N → N+1** | ensaiar a **virada** entre atos (§19, Ensaio 3) |

Sem isso, ensaiar a passagem do Ato 4 para o 5 exigiria rodar o set inteiro e esperar ~50 min
chegar lá. O trecho é compilado **como se fosse um set próprio**: abre no deck 1 e não tenta
entregar para uma faixa que não está nele.

**Pause / Resume / Skip / Stop set** controlam a execução. **Mover o crossfader** entra em
*Manual override* e devolve os decks. O set **continua tocando se você fechar o diálogo** — só a
prévia é cancelada.

- **Preview on decks**: carrega A no deck 1 e B no deck 2, posiciona nos pontos planejados e
  executa a automação (crossfader/volumes/EQ) guiada pela **posição de reprodução** do deck A —
  não por relógio.
- **Sync só quando faz sentido**: nas mesclas (EQ Blend, Bass Swap, Crossfade, Filter) o deck B é
  beat-matchado ao A. Em **Cut on phrase** e **Auto DJ fallback**, **não** — essas transições
  existem justamente porque os tempos são incompatíveis, e sincronizar arrastaria a faixa nova
  para um tempo estranho (ex.: 140 BPM puxada para 112 = 20% de stretch). Cada faixa mantém o
  tempo dela e a troca é na frase.
- **Repeat**: re-posiciona e executa de novo.
- **Cancel preview**: para a automação **sem cortar o áudio** e devolve os decks.
- **Fechar o diálogo cancela** um preview em andamento.
- **Mexer no crossfader durante o preview** → estado **Manual override**: o módulo solta os
  controles e não briga com você.

Estados: `Idle → Loading → Transitioning → Completed` (ou `Cancelled` / `Manual override` / `Failed`).

---

## Problemas comuns

**Energy / Sections / Exit @ vazios ("—") e tudo vira Auto DJ fallback**
A faixa não tem **waveform** armazenado. Analise-a pela biblioteca do Mixxx (clique direito →
*Analisar*) e depois clique em **Read native analysis from library**.

**Transições não mudaram depois de atualizar o módulo**
Você clicou em *Reload snapshots* (que só relê o gravado). Clique em
**Read native analysis from library** para recalcular.

**Ajustei uma transição e ela voltou ao automático**
O ajuste é gravado por **par de faixas**. Se o par deixou de existir na sequência (você reordenou
e aquelas duas faixas não são mais vizinhas), o ajuste continua salvo, mas não se aplica a nada —
ele volta a valer se o par voltar a existir. Se você quis mesmo desfazer, escolha **Automatic**.

**Forcei 64 compassos e saiu menos**
A transição não cabia antes da faixa que sai terminar. O relatório mostra o aviso
`… does not fit before the track ends; trimmed to N`. Escolha um número menor ou uma faixa com
mais cauda depois do `Exit @`.

**Mudei/limpei a biblioteca, mas o Generate sequence mostra as faixas antigas**
Os snapshots vivem no **sidecar**, não na biblioteca — limpar a biblioteca do Mixxx não mexe
neles. Clique em **Read native analysis from library**: além de recalcular, ele agora **remove do
sidecar as faixas que não existem mais** na biblioteca. Para um reset total, use
**Clear snapshots**.

**Uma faixa com BPM absurdo (ex.: 187.5 numa música de 112)**
É erro de **beatgrid do Mixxx**, não do módulo. O planejador reage certo (avisa
*"tempo change above tolerance"* e usa **Cut on phrase**), mas corrija o beatgrid da faixa
ou tire-a do conjunto.

**Áudio picotando/cortando**
Configuração de saída, não o módulo (ele nunca toca a thread de áudio). Veja *Pré-requisitos*:
dispositivo físico em vez de virtual, WASAPI, taxa de amostragem casada, e — se só picota com os
dois decks juntos — troque *Trava de Tom* de **Rubberband** para **Soundtouch** (menos CPU ao
esticar duas faixas sincronizadas).

---

## Onde ficam os dados

- **Sidecar**: `%LOCALAPPDATA%\Mixxx\music-sync-dj.sqlite` (schema v4) — snapshots e settings do
  módulo. Separado da biblioteca do Mixxx; apagá-lo só perde os snapshots (é só re-snapshotar).
- A biblioteca, os waveforms e a análise nativa continuam sendo do **Mixxx** — o módulo lê, não
  reescreve.

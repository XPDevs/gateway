/**
 * Gateway | Main Application
 * XPDevs | https://xpdevs.github.io
 */
(function() {
    'use strict';

    const $ = id => document.getElementById(id);

    // ======================== TRANSLATIONS ========================

    const LANG = {
        en: { about:'About', images:'Images', searchPlaceholder:'Search the Gateway index, ask Quick Wiki, or type a URL', gatewaySearch:'Gateway Search', feelingLucky:"I'm Feeling Lucky", unitedKingdom:'United Kingdom', advertising:'Advertising', business:'Business', howSearchWorks:'How Search works', privacy:'Privacy', terms:'Terms', settings:'Settings', all:'All', showMoreResults:'Show more results', noResultsFound:'No results found', tryDifferentKeywords:'Try different keywords or check your spelling', didYouMean:'Did you mean:', quickAnswer:'Quick Answer', quickWiki:'Quick Wiki', quickWikiExample:'Try "release date of PlayStation 5"', quickWikiHint:'Ask a question for a sourced, deterministic answer', indexLoading:'Loading the local web index…', indexPages:'{count} pages in the local index', searchingMultipleSources:'Searching the Gateway index and open sources…', connectionError:'Connection error. Please check your internet and try again.', tagline:'Your own index of the web.', resultsStats:'{count} ranked results', settingsTitle:'Settings', language:'Language', darkMode:'Dark mode', close:'Close', wikipedia:'Wikipedia', openData:'Open data', noGenerativeAi:'No generative AI' },
        es: { about:'Acerca de', images:'Imágenes', searchPlaceholder:'Buscar en Gateway o escribe una URL', gatewaySearch:'Buscar con Gateway', feelingLucky:'Voy a tener suerte', unitedKingdom:'Reino Unido', advertising:'Publicidad', business:'Negocios', howSearchWorks:'Cómo funciona la Búsqueda', privacy:'Privacidad', terms:'Términos', settings:'Configuración', all:'Todo', showMoreResults:'Mostrar más resultados', noResultsFound:'No se encontraron resultados', tryDifferentKeywords:'Prueba con otras palabras o revisa la ortografía', didYouMean:'Quizás quisiste decir:', quickAnswer:'Respuesta rápida', searchingMultipleSources:'Buscando en múltiples fuentes...', connectionError:'Error de conexión. Verifica tu internet e inténtalo de nuevo.', tagline:'Más rápido que Google. Mismos resultados.', resultsStats:'Aprox. {count} resultados', settingsTitle:'Configuración', language:'Idioma', darkMode:'Modo oscuro', close:'Cerrar', wikipedia:'Wikipedia' },
        fr: { about:'À propos', images:'Images', searchPlaceholder:'Rechercher sur Gateway ou saisir une URL', gatewaySearch:'Recherche Gateway', feelingLucky:'J\'ai de la chance', unitedKingdom:'Royaume-Uni', advertising:'Publicité', business:'Entreprises', howSearchWorks:'Fonctionnement de la Recherche', privacy:'Confidentialité', terms:'Conditions', settings:'Paramètres', all:'Tout', showMoreResults:'Plus de résultats', noResultsFound:'Aucun résultat trouvé', tryDifferentKeywords:'Essayez différents mots-clés ou vérifiez l\'orthographe', didYouMean:'Vouliez-vous dire :', quickAnswer:'Réponse rapide', searchingMultipleSources:'Recherche multi-sources...', connectionError:'Erreur de connexion. Vérifiez votre connexion et réessayez.', tagline:'Plus rapide que Google. Mêmes résultats.', resultsStats:'Environ {count} résultats', settingsTitle:'Paramètres', language:'Langue', darkMode:'Mode sombre', close:'Fermer', wikipedia:'Wikipédia' },
        de: { about:'Über uns', images:'Bilder', searchPlaceholder:'Gateway durchsuchen oder URL eingeben', gatewaySearch:'Gateway-Suche', feelingLucky:'Auf gut Glück', unitedKingdom:'Vereinigtes Königreich', advertising:'Werbung', business:'Unternehmen', howSearchWorks:'So funktioniert die Suche', privacy:'Datenschutz', terms:'AGB', settings:'Einstellungen', all:'Alle', showMoreResults:'Weitere Ergebnisse', noResultsFound:'Keine Ergebnisse gefunden', tryDifferentKeywords:'Versuchen Sie andere Suchbegriffe oder überprüfen Sie die Rechtschreibung', didYouMean:'Meinten Sie:', quickAnswer:'Kurze Antwort', searchingMultipleSources:'Durchsuche mehrere Quellen...', connectionError:'Verbindungsfehler. Bitte Internet prüfen und erneut versuchen.', tagline:'Schneller als Google. Gleiche Ergebnisse.', resultsStats:'Ca. {count} Ergebnisse', settingsTitle:'Einstellungen', language:'Sprache', darkMode:'Dunkelmodus', close:'Schließen', wikipedia:'Wikipedia' },
        it: { about:'Informazioni', images:'Immagini', searchPlaceholder:'Cerca su Gateway o digita un URL', gatewaySearch:'Cerca con Gateway', feelingLucky:'Mi sento fortunato', unitedKingdom:'Regno Unito', advertising:'Pubblicità', business:'Business', howSearchWorks:'Come funziona la Ricerca', privacy:'Privacy', terms:'Termini', settings:'Impostazioni', all:'Tutto', showMoreResults:'Mostra altri risultati', noResultsFound:'Nessun risultato trovato', tryDifferentKeywords:'Prova con parole diverse o controlla l\'ortografia', didYouMean:'Forse cercavi:', quickAnswer:'Risposta rapida', searchingMultipleSources:'Ricerca in più fonti...', connectionError:'Errore di connessione. Controlla la connessione e riprova.', tagline:'Più veloce di Google. Stessi risultati.', resultsStats:'Circa {count} risultati', settingsTitle:'Impostazioni', language:'Lingua', darkMode:'Modalità scura', close:'Chiudi', wikipedia:'Wikipedia' },
        pt: { about:'Sobre', images:'Imagens', searchPlaceholder:'Pesquisar Gateway ou digitar URL', gatewaySearch:'Pesquisa Gateway', feelingLucky:'Estou com sorte', unitedKingdom:'Reino Unido', advertising:'Publicidade', business:'Negócios', howSearchWorks:'Como funciona a Pesquisa', privacy:'Privacidade', terms:'Termos', settings:'Configurações', all:'Tudo', showMoreResults:'Mostrar mais resultados', noResultsFound:'Nenhum resultado encontrado', tryDifferentKeywords:'Tente palavras-chave diferentes ou verifique a ortografia', didYouMean:'Você quis dizer:', quickAnswer:'Resposta rápida', searchingMultipleSources:'Pesquisando em múltiplas fontes...', connectionError:'Erro de conexão. Verifique sua internet e tente novamente.', tagline:'Mais rápido que o Google. Mesmos resultados.', resultsStats:'Aprox. {count} resultados', settingsTitle:'Configurações', language:'Idioma', darkMode:'Modo escuro', close:'Fechar', wikipedia:'Wikipédia' },
        ru: { about:'О нас', images:'Картинки', searchPlaceholder:'Поиск в Gateway или введите URL', gatewaySearch:'Поиск Gateway', feelingLucky:'Мне повезёт', unitedKingdom:'Великобритания', advertising:'Реклама', business:'Бизнес', howSearchWorks:'Как работает поиск', privacy:'Конфиденциальность', terms:'Условия', settings:'Настройки', all:'Все', showMoreResults:'Показать больше', noResultsFound:'Ничего не найдено', tryDifferentKeywords:'Попробуйте другие слова или проверьте орфографию', didYouMean:'Возможно, вы имели в виду:', quickAnswer:'Быстрый ответ', searchingMultipleSources:'Поиск по нескольким источникам...', connectionError:'Ошибка подключения. Проверьте интернет и повторите попытку.', tagline:'Быстрее Google. Те же результаты.', resultsStats:'Примерно {count} результатов', settingsTitle:'Настройки', language:'Язык', darkMode:'Тёмная тема', close:'Закрыть', wikipedia:'Википедия' },
        ja: { about:'概要', images:'画像', searchPlaceholder:'Gatewayを検索、またはURLを入力', gatewaySearch:'Gateway検索', feelingLucky:'I\'m Feeling Lucky', unitedKingdom:'イギリス', advertising:'広告', business:'ビジネス', howSearchWorks:'検索の仕組み', privacy:'プライバシー', terms:'利用規約', settings:'設定', all:'すべて', showMoreResults:'さらに表示', noResultsFound:'結果が見つかりませんでした', tryDifferentKeywords:'別のキーワードを試すか、スペルを確認してください', didYouMean:'もしかして:', quickAnswer:'クイックアンサー', searchingMultipleSources:'複数のソースを検索中...', connectionError:'接続エラーです。インターネット接続を確認してもう一度お試しください。', tagline:'Googleより高速。同じ結果。', resultsStats:'約{count}件', settingsTitle:'設定', language:'言語', darkMode:'ダークモード', close:'閉じる', wikipedia:'ウィキペディア' },
        'zh-CN': { about:'关于', images:'图片', searchPlaceholder:'搜索 Gateway 或输入网址', gatewaySearch:'Gateway 搜索', feelingLucky:'手气不错', unitedKingdom:'英国', advertising:'广告', business:'商务', howSearchWorks:'搜索工作原理', privacy:'隐私', terms:'条款', settings:'设置', all:'全部', showMoreResults:'显示更多结果', noResultsFound:'未找到结果', tryDifferentKeywords:'尝试不同的关键词或检查拼写', didYouMean:'您是不是要找：', quickAnswer:'快速解答', searchingMultipleSources:'正在搜索多个来源...', connectionError:'连接错误。请检查网络后重试。', tagline:'比谷歌更快。相同结果。', resultsStats:'约{count}条结果', settingsTitle:'设置', language:'语言', darkMode:'深色模式', close:'关闭', wikipedia:'维基百科' },
        'zh-TW': { about:'關於', images:'圖片', searchPlaceholder:'搜尋 Gateway 或輸入網址', gatewaySearch:'Gateway 搜尋', feelingLucky:'好手氣', unitedKingdom:'英國', advertising:'廣告', business:'商務', howSearchWorks:'搜尋運作方式', privacy:'隱私權', terms:'條款', settings:'設定', all:'全部', showMoreResults:'顯示更多結果', noResultsFound:'找不到結果', tryDifferentKeywords:'請嘗試不同的關鍵字或檢查拼寫', didYouMean:'您是不是要找：', quickAnswer:'快速解答', searchingMultipleSources:'正在搜尋多個來源...', connectionError:'連線錯誤。請檢查網路後重試。', tagline:'比 Google 更快。相同結果。', resultsStats:'約{count}項結果', settingsTitle:'設定', language:'語言', darkMode:'深色模式', close:'關閉', wikipedia:'維基百科' },
        ko: { about:'정보', images:'이미지', searchPlaceholder:'Gateway 검색 또는 URL 입력', gatewaySearch:'Gateway 검색', feelingLucky:'행운을 빌어요', unitedKingdom:'영국', advertising:'광고', business:'비즈니스', howSearchWorks:'검색 작동 방식', privacy:'개인정보', terms:'약관', settings:'설정', all:'전체', showMoreResults:'더 많은 결과 보기', noResultsFound:'검색 결과가 없습니다', tryDifferentKeywords:'다른 키워드를 시도하거나 철자를 확인하세요', didYouMean:'혹시 찾으시는 것이:', quickAnswer:'빠른 답변', searchingMultipleSources:'여러 소스 검색 중...', connectionError:'연결 오류입니다. 인터넷을 확인하고 다시 시도하세요.', tagline:'Google보다 빠름. 동일한 결과.', resultsStats:'약 {count}개 결과', settingsTitle:'설정', language:'언어', darkMode:'다크 모드', close:'닫기', wikipedia:'위키백과' },
        ar: { about:'حول', images:'صور', searchPlaceholder:'ابحث في Gateway أو أدخل رابطاً', gatewaySearch:'بحث Gateway', feelingLucky:'أنا محظوظ', unitedKingdom:'المملكة المتحدة', advertising:'إعلانات', business:'أعمال', howSearchWorks:'كيف يعمل البحث', privacy:'خصوصية', terms:'الشروط', settings:'إعدادات', all:'الكل', showMoreResults:'عرض المزيد من النتائج', noResultsFound:'لم يتم العثور على نتائج', tryDifferentKeywords:'جرّب كلمات مختلفة أو تحقق من الإملاء', didYouMean:'هل تقصد:', quickAnswer:'إجابة سريعة', searchingMultipleSources:'جاري البحث في مصادر متعددة...', connectionError:'خطأ في الاتصال. تحقق من اتصالك بالإنترنت وحاول مرة أخرى.', tagline:'أسرع من Google. نفس النتائج.', resultsStats:'حوالي {count} نتيجة', settingsTitle:'الإعدادات', language:'اللغة', darkMode:'الوضع الداكن', close:'إغلاق', wikipedia:'ويكيبيديا' },
        hi: { about:'बारे में', images:'चित्र', searchPlaceholder:'Gateway में खोजें या URL टाइप करें', gatewaySearch:'Gateway खोज', feelingLucky:'मैं भाग्यशाली हूँ', unitedKingdom:'यूनाइटेड किंगडम', advertising:'विज्ञापन', business:'व्यवसाय', howSearchWorks:'खोज कैसे काम करती है', privacy:'गोपनीयता', terms:'शर्तें', settings:'सेटिंग्स', all:'सभी', showMoreResults:'और परिणाम दिखाएँ', noResultsFound:'कोई परिणाम नहीं मिला', tryDifferentKeywords:'अलग कीवर्ड आज़माएँ या वर्तनी जाँचें', didYouMean:'क्या आप यह कहना चाह रहे थे:', quickAnswer:'त्वरित उत्तर', searchingMultipleSources:'कई स्रोतों में खोज रहे हैं...', connectionError:'कनेक्शन त्रुटि। कृपया अपना इंटरनेट जाँचें और पुनः प्रयास करें।', tagline:'Google से तेज़। वही परिणाम।', resultsStats:'लगभग {count} परिणाम', settingsTitle:'सेटिंग्स', language:'भाषा', darkMode:'डार्क मोड', close:'बंद करें', wikipedia:'विकिपीडिया' },
        bn: { about:'সম্পর্কে', images:'ছবি', searchPlaceholder:'Gateway-এ অনুসন্ধান করুন বা URL লিখুন', gatewaySearch:'Gateway অনুসন্ধান', feelingLucky:'আমি ভাগ্যবান', unitedKingdom:'যুক্তরাজ্য', advertising:'বিজ্ঞাপন', business:'ব্যবসা', howSearchWorks:'কিভাবে অনুসন্ধান কাজ করে', privacy:'গোপনীয়তা', terms:'শর্তাবলী', settings:'সেটিংস', all:'সব', showMoreResults:'আরও ফলাফল দেখান', noResultsFound:'কোনো ফলাফল পাওয়া যায়নি', tryDifferentKeywords:'ভিন্ন শব্দ ব্যবহার করুন বা বানান পরীক্ষা করুন', didYouMean:'আপনি কি বোঝাতে চেয়েছেন:', quickAnswer:'দ্রুত উত্তর', searchingMultipleSources:'একাধিক উৎসে অনুসন্ধান করা হচ্ছে...', connectionError:'সংযোগ ত্রুটি। আপনার ইন্টারনেট পরীক্ষা করে আবার চেষ্টা করুন।', tagline:'Google-এর চেয়ে দ্রুত। একই ফলাফল।', resultsStats:'প্রায় {count}টি ফলাফল', settingsTitle:'সেটিংস', language:'ভাষা', darkMode:'ডার্ক মোড', close:'বন্ধ করুন', wikipedia:'উইকিপিডিয়া' },
        tr: { about:'Hakkında', images:'Görseller', searchPlaceholder:'Gateway\'de ara veya URL yaz', gatewaySearch:'Gateway Arama', feelingLucky:'Şanslıyım', unitedKingdom:'Birleşik Krallık', advertising:'Reklam', business:'İşletme', howSearchWorks:'Arama nasıl çalışır', privacy:'Gizlilik', terms:'Şartlar', settings:'Ayarlar', all:'Tümü', showMoreResults:'Daha fazla sonuç göster', noResultsFound:'Sonuç bulunamadı', tryDifferentKeywords:'Farklı anahtar kelimeler deneyin veya yazımı kontrol edin', didYouMean:'Bunu mu demek istediniz:', quickAnswer:'Hızlı Cevap', searchingMultipleSources:'Birden çok kaynak taranıyor...', connectionError:'Bağlantı hatası. Lütfen internetinizi kontrol edip tekrar deneyin.', tagline:'Google\'dan daha hızlı. Aynı sonuçlar.', resultsStats:'Yaklaşık {count} sonuç', settingsTitle:'Ayarlar', language:'Dil', darkMode:'Karanlık mod', close:'Kapat', wikipedia:'Vikipedi' },
        nl: { about:'Over ons', images:'Afbeeldingen', searchPlaceholder:'Zoek op Gateway of typ een URL', gatewaySearch:'Gateway Zoeken', feelingLucky:'Gelukzoeker', unitedKingdom:'Verenigd Koninkrijk', advertising:'Adverteren', business:'Zakelijk', howSearchWorks:'Hoe zoeken werkt', privacy:'Privacy', terms:'Voorwaarden', settings:'Instellingen', all:'Alles', showMoreResults:'Meer resultaten', noResultsFound:'Geen resultaten gevonden', tryDifferentKeywords:'Probeer andere zoekwoorden of controleer de spelling', didYouMean:'Bedoelde u:', quickAnswer:'Snel antwoord', searchingMultipleSources:'Meerdere bronnen doorzoeken...', connectionError:'Verbindingsfout. Controleer uw internet en probeer het opnieuw.', tagline:'Sneller dan Google. Zelfde resultaten.', resultsStats:'Ongeveer {count} resultaten', settingsTitle:'Instellingen', language:'Taal', darkMode:'Donkere modus', close:'Sluiten', wikipedia:'Wikipedia' },
        pl: { about:'O nas', images:'Obrazy', searchPlaceholder:'Szukaj w Gateway lub wpisz URL', gatewaySearch:'Szukaj w Gateway', feelingLucky:'Szczęściarz', unitedKingdom:'Wielka Brytania', advertising:'Reklama', business:'Firmy', howSearchWorks:'Jak działa wyszukiwanie', privacy:'Prywatność', terms:'Warunki', settings:'Ustawienia', all:'Wszystkie', showMoreResults:'Pokaż więcej wyników', noResultsFound:'Brak wyników', tryDifferentKeywords:'Spróbuj innych słów kluczowych lub sprawdź pisownię', didYouMean:'Czy chodziło Ci o:', quickAnswer:'Szybka odpowiedź', searchingMultipleSources:'Przeszukiwanie wielu źródeł...', connectionError:'Błąd połączenia. Sprawdź internet i spróbuj ponownie.', tagline:'Szybsze niż Google. Te same wyniki.', resultsStats:'Około {count} wyników', settingsTitle:'Ustawienia', language:'Język', darkMode:'Tryb ciemny', close:'Zamknij', wikipedia:'Wikipedia' },
        sv: { about:'Om', images:'Bilder', searchPlaceholder:'Sök på Gateway eller skriv en URL', gatewaySearch:'Gateway-sökning', feelingLucky:'Jag känner mig turlig', unitedKingdom:'Storbritannien', advertising:'Annonsering', business:'Företag', howSearchWorks:'Så fungerar sökning', privacy:'Integritet', terms:'Villkor', settings:'Inställningar', all:'Alla', showMoreResults:'Visa fler resultat', noResultsFound:'Inga resultat hittades', tryDifferentKeywords:'Prova andra sökord eller kontrollera stavningen', didYouMean:'Menade du:', quickAnswer:'Snabbt svar', searchingMultipleSources:'Söker i flera källor...', connectionError:'Anslutningsfel. Kontrollera din internetanslutning och försök igen.', tagline:'Snabbare än Google. Samma resultat.', resultsStats:'Ungefär {count} resultat', settingsTitle:'Inställningar', language:'Språk', darkMode:'Mörkt läge', close:'Stäng', wikipedia:'Wikipedia' },
        da: { about:'Om', images:'Billeder', searchPlaceholder:'Søg på Gateway eller skriv en URL', gatewaySearch:'Gateway-søgning', feelingLucky:'Jeg er heldig', unitedKingdom:'Storbritannien', advertising:'Annoncering', business:'Virksomhed', howSearchWorks:'Sådan fungerer søgning', privacy:'Privatliv', terms:'Vilkår', settings:'Indstillinger', all:'Alle', showMoreResults:'Vis flere resultater', noResultsFound:'Ingen resultater fundet', tryDifferentKeywords:'Prøv andre søgeord eller tjek stavningen', didYouMean:'Mente du:', quickAnswer:'Hurtigt svar', searchingMultipleSources:'Søger i flere kilder...', connectionError:'Forbindelsesfejl. Tjek din internetforbindelse og prøv igen.', tagline:'Hurtigere end Google. Samme resultater.', resultsStats:'Ca. {count} resultater', settingsTitle:'Indstillinger', language:'Sprog', darkMode:'Mørk tilstand', close:'Luk', wikipedia:'Wikipedia' },
        fi: { about:'Tietoja', images:'Kuvat', searchPlaceholder:'Hae Gatewaysta tai kirjoita URL', gatewaySearch:'Gateway-haku', feelingLucky:'Minulla on tuuria', unitedKingdom:'Yhdistynyt kuningaskunta', advertising:'Mainonta', business:'Yritykset', howSearchWorks:'Näin haku toimii', privacy:'Yksityisyys', terms:'Ehdot', settings:'Asetukset', all:'Kaikki', showMoreResults:'Näytä lisää tuloksia', noResultsFound:'Ei tuloksia', tryDifferentKeywords:'Kokeile eri hakusanoja tai tarkista oikeinkirjoitus', didYouMean:'Tarkoititko:', quickAnswer:'Pikavastaus', searchingMultipleSources:'Haetaan useista lähteistä...', connectionError:'Yhteysvirhe. Tarkista internetyhteys ja yritä uudelleen.', tagline:'Nopeampi kuin Google. Samat tulokset.', resultsStats:'Noin {count} tulosta', settingsTitle:'Asetukset', language:'Kieli', darkMode:'Tumma tila', close:'Sulje', wikipedia:'Wikipedia' },
        no: { about:'Om', images:'Bilder', searchPlaceholder:'Søk på Gateway eller skriv en URL', gatewaySearch:'Gateway-søk', feelingLucky:'Jeg er heldig', unitedKingdom:'Storbritannia', advertising:'Annonsering', business:'Bedrifter', howSearchWorks:'Slik fungerer søk', privacy:'Personvern', terms:'Vilkår', settings:'Innstillinger', all:'Alle', showMoreResults:'Vis flere resultater', noResultsFound:'Ingen resultater funnet', tryDifferentKeywords:'Prøv andre søkeord eller sjekk stavemåten', didYouMean:'Mente du:', quickAnswer:'Hurtig svar', searchingMultipleSources:'Søker i flere kilder...', connectionError:'Tilkoblingsfeil. Sjekk internettilkoblingen og prøv igjen.', tagline:'Raskere enn Google. Samme resultater.', resultsStats:'Omtrent {count} resultater', settingsTitle:'Innstillinger', language:'Språk', darkMode:'Mørk modus', close:'Lukk', wikipedia:'Wikipedia' },
        cs: { about:'O nás', images:'Obrázky', searchPlaceholder:'Hledat na Gateway nebo zadat URL', gatewaySearch:'Hledat Gateway', feelingLucky:'Chci mít štěstí', unitedKingdom:'Spojené království', advertising:'Reklama', business:'Firmy', howSearchWorks:'Jak vyhledávání funguje', privacy:'Soukromí', terms:'Smluvní podmínky', settings:'Nastavení', all:'Vše', showMoreResults:'Zobrazit více výsledků', noResultsFound:'Nebyly nalezeny žádné výsledky', tryDifferentKeywords:'Zkuste jiná klíčová slova nebo zkontrolujte pravopis', didYouMean:'Mysleli jste:', quickAnswer:'Rychlá odpověď', searchingMultipleSources:'Vyhledávání ve více zdrojích...', connectionError:'Chyba připojení. Zkontrolujte připojení k internetu a zkuste to znovu.', tagline:'Rychlejší než Google. Stejné výsledky.', resultsStats:'Přibližně {count} výsledků', settingsTitle:'Nastavení', language:'Jazyk', darkMode:'Tmavý režim', close:'Zavřít', wikipedia:'Wikipedie' },
        ro: { about:'Despre', images:'Imagini', searchPlaceholder:'Căutați pe Gateway sau introduceți o adresă URL', gatewaySearch:'Căutare Gateway', feelingLucky:'Norocos', unitedKingdom:'Regatul Unit', advertising:'Publicitate', business:'Afaceri', howSearchWorks:'Cum funcționează Căutarea', privacy:'Confidențialitate', terms:'Termeni', settings:'Setări', all:'Toate', showMoreResults:'Arată mai multe rezultate', noResultsFound:'Nu s-au găsit rezultate', tryDifferentKeywords:'Încercați alte cuvinte cheie sau verificați ortografia', didYouMean:'Poate ați vrut să spuneți:', quickAnswer:'Răspuns rapid', searchingMultipleSources:'Se caută în mai multe surse...', connectionError:'Eroare de conexiune. Verificați internetul și încercați din nou.', tagline:'Mai rapid decât Google. Aceleași rezultate.', resultsStats:'Aproximativ {count} rezultate', settingsTitle:'Setări', language:'Limbă', darkMode:'Mod întunecat', close:'Închide', wikipedia:'Wikipedia' },
        hu: { about:'Névjegy', images:'Képek', searchPlaceholder:'Keresés a Gatewayben vagy URL megadása', gatewaySearch:'Gateway-keresés', feelingLucky:'Szerencsém van', unitedKingdom:'Egyesült Királyság', advertising:'Hirdetés', business:'Vállalkozások', howSearchWorks:'Hogyan működik a keresés', privacy:'Adatvédelem', terms:'Feltételek', settings:'Beállítások', all:'Összes', showMoreResults:'Több találat mutatása', noResultsFound:'Nincs találat', tryDifferentKeywords:'Próbáljon más kulcsszavakat vagy ellenőrizze a helyesírást', didYouMean:'Esetleg erre gondolt:', quickAnswer:'Gyors válasz', searchingMultipleSources:'Keresés több forrásban...', connectionError:'Kapcsolódási hiba. Ellenőrizze az internetkapcsolatot és próbálja újra.', tagline:'Gyorsabb, mint a Google. Ugyanazok az eredmények.', resultsStats:'Kb. {count} találat', settingsTitle:'Beállítások', language:'Nyelv', darkMode:'Sötét mód', close:'Bezár', wikipedia:'Wikipédia' },
        el: { about:'Σχετικά', images:'Εικόνες', searchPlaceholder:'Αναζήτηση στο Gateway ή πληκτρολογήστε URL', gatewaySearch:'Αναζήτηση Gateway', feelingLucky:'Τυχερός', unitedKingdom:'Ηνωμένο Βασίλειο', advertising:'Διαφήμιση', business:'Επιχειρήσεις', howSearchWorks:'Πώς λειτουργεί η Αναζήτηση', privacy:'Απόρρητο', terms:'Όροι', settings:'Ρυθμίσεις', all:'Όλα', showMoreResults:'Εμφάνιση περισσότερων αποτελεσμάτων', noResultsFound:'Δεν βρέθηκαν αποτελέσματα', tryDifferentKeywords:'Δοκιμάστε διαφορετικές λέξεις-κλειδιά ή ελέγξτε την ορθογραφία', didYouMean:'Μήπως εννοείτε:', quickAnswer:'Γρήγορη απάντηση', searchingMultipleSources:'Αναζήτηση σε πολλαπλές πηγές...', connectionError:'Σφάλμα σύνδεσης. Ελέγξτε το διαδίκτυο και δοκιμάστε ξανά.', tagline:'Ταχύτερο από το Google. Ίδια αποτελέσματα.', resultsStats:'Περίπου {count} αποτελέσματα', settingsTitle:'Ρυθμίσεις', language:'Γλώσσα', darkMode:'Σκοτεινή λειτουργία', close:'Κλείσιμο', wikipedia:'Βικιπαίδεια' }
    };

    // These labels are shared by the search UI and were added after the
    // original locale table. Keep the table complete so a language switch
    // never silently falls back to an English loading state or AI badge.
    const QUICK_COPY = {
        en: { quickWiki:'Quick Wiki', quickWikiExample:'Try "release date of PlayStation 5"', quickWikiHint:'Ask a question for a sourced, deterministic answer', indexLoading:'Loading the local web index…', indexPages:'{count} pages in the local index', openData:'Open data', noGenerativeAi:'No generative AI' },
        es: { quickWiki:'Wiki rápida', quickWikiExample:'Prueba "fecha de lanzamiento de PlayStation 5"', quickWikiHint:'Haz una pregunta para obtener una respuesta determinista y con fuentes', indexLoading:'Cargando el índice web local…', indexPages:'{count} páginas en el índice local', openData:'Datos abiertos', noGenerativeAi:'Sin IA generativa' },
        fr: { quickWiki:'Wiki rapide', quickWikiExample:'Essayez « date de sortie de PlayStation 5 »', quickWikiHint:'Posez une question pour une réponse sourcée et déterministe', indexLoading:'Chargement de l’index web local…', indexPages:'{count} pages dans l’index local', openData:'Données ouvertes', noGenerativeAi:'Pas d’IA générative' },
        de: { quickWiki:'Schnellwiki', quickWikiExample:'„Veröffentlichungsdatum der PlayStation 5“ ausprobieren', quickWikiHint:'Stelle eine Frage für eine determinierte Antwort mit Quellen', indexLoading:'Lokaler Web-Index wird geladen…', indexPages:'{count} Seiten im lokalen Index', openData:'Offene Daten', noGenerativeAi:'Keine generative KI' },
        it: { quickWiki:'Wiki rapida', quickWikiExample:'Prova "data di uscita di PlayStation 5"', quickWikiHint:'Fai una domanda per una risposta deterministica e sourced', indexLoading:'Caricamento dell’indice web locale…', indexPages:'{count} pagine nell’indice locale', openData:'Dati aperti', noGenerativeAi:'Nessuna IA generativa' },
        pt: { quickWiki:'Wiki rápida', quickWikiExample:'Experimente "data de lançamento do PlayStation 5"', quickWikiHint:'Faça uma pergunta para obter uma resposta determinística com fontes', indexLoading:'A carregar o índice web local…', indexPages:'{count} páginas no índice local', openData:'Dados abertos', noGenerativeAi:'Sem IA generativa' },
        ru: { quickWiki:'Быстрая Вики', quickWikiExample:'Попробуйте «дата выхода PlayStation 5»', quickWikiHint:'Задайте вопрос для детерминированного ответа со ссылками', indexLoading:'Загрузка локального веб-индекса…', indexPages:'{count} страниц в локальном индексе', openData:'Открытые данные', noGenerativeAi:'Без генеративного ИИ' },
        ja: { quickWiki:'クイックウィキ', quickWikiExample:'「PlayStation 5の発売日」を試す', quickWikiHint:'出典付きの決定的な回答を質問できます', indexLoading:'ローカルウェブインデックスを読み込み中…', indexPages:'ローカルインデックスに{count}ページ', openData:'オープンデータ', noGenerativeAi:'生成AI不使用' },
        'zh-CN': { quickWiki:'快速维基', quickWikiExample:'试试“PlayStation 5 何时发布”', quickWikiHint:'提问以获取有来源的确定性答案', indexLoading:'正在加载本地网页索引…', indexPages:'本地索引中有 {count} 个页面', openData:'开放数据', noGenerativeAi:'不使用生成式 AI' },
        'zh-TW': { quickWiki:'快速維基', quickWikiExample:'試試「PlayStation 5 什麼時候 release」', quickWikiHint:'提問以取得有來源且具決定性的答案', indexLoading:'正在載入本機網頁索引…', indexPages:'本機索引中有 {count} 個頁面', openData:'開放資料', noGenerativeAi:'不使用生成式 AI' },
        ko: { quickWiki:'빠른 위키', quickWikiExample:'“PlayStation 5 출시일”을 입력해 보세요', quickWikiHint:'출처가 있는 결정적 답변을 질문하세요', indexLoading:'로컬 웹 인덱스를 불러오는 중…', indexPages:'로컬 인덱스에 {count}개 페이지', openData:'공개 데이터', noGenerativeAi:'생성형 AI 없음' },
        ar: { quickWiki:'ويكي سريع', quickWikiExample:'جرّب "تاريخ إصدار PlayStation 5"', quickWikiHint:'اطرح سؤالًا للحصول على إجابة موثقة وحتمية', indexLoading:'جارٍ تحميل فهرس الويب المحلي…', indexPages:'{count} صفحة في الفهرس المحلي', openData:'بيانات مفتوحة', noGenerativeAi:'لا يوجد ذكاء اصطناعي توليدي' },
        hi: { quickWiki:'त्वरित विकी', quickWikiExample:'"PlayStation 5 कब जारी हुआ" आज़माएँ', quickWikiHint:'स्रोत सहित निश्चित उत्तर के लिए प्रश्न पूछें', indexLoading:'स्थानीय वेब इंडेक्स लोड हो रहा है…', indexPages:'स्थानीय इंडेक्स में {count} पृष्ठ', openData:'खुला डेटा', noGenerativeAi:'कोई जनरेटिव AI नहीं' },
        bn: { quickWiki:'দ্রুত উইকি', quickWikiExample:'“PlayStation 5 কখন প্রকাশিত হয়” চেষ্টা করুন', quickWikiHint:'উৎসসহ নির্ধারিত উত্তর পেতে প্রশ্ন করুন', indexLoading:'স্থানীয় ওয়েব ইনডেক্স লোড হচ্ছে…', indexPages:'স্থানীয় ইনডেক্সে {count}টি পৃষ্ঠা', openData:'উন্মুক্ত ডেটা', noGenerativeAi:'কোনো জেনারেটিভ AI নয়' },
        tr: { quickWiki:'Hızlı wiki', quickWikiExample:'"PlayStation 5 ne zaman çıktı" sorusunu deneyin', quickWikiHint:'Kaynaklı ve deterministik bir yanıt için soru sorun', indexLoading:'Yerel web dizini yükleniyor…', indexPages:'Yerel dizinde {count} sayfa', openData:'Açık veri', noGenerativeAi:'Üretken yapay zekâ yok' },
        nl: { quickWiki:'Snelle wiki', quickWikiExample:'Probeer "releasedatum PlayStation 5"', quickWikiHint:'Stel een vraag voor een gedetermineerd antwoord met bronnen', indexLoading:'Lokale webindex laden…', indexPages:'{count} pagina’s in de lokale index', openData:'Open gegevens', noGenerativeAi:'Geen generatieve AI' },
        pl: { quickWiki:'Szybka wiki', quickWikiExample:'Spróbuj „data premiery PlayStation 5”', quickWikiHint:'Zadaj pytanie, aby otrzymać deterministyczną odpowiedź ze źródłami', indexLoading:'Ładowanie lokalnego indeksu stron…', indexPages:'{count} stron w lokalnym indeksie', openData:'Otwarte dane', noGenerativeAi:'Bez generatywnej AI' },
        sv: { quickWiki:'Snabb wiki', quickWikiExample:'Prova ”releasedatum för PlayStation 5”', quickWikiHint:'Ställ en fråga för ett källbelagt, deterministiskt svar', indexLoading:'Laddar lokal webindex…', indexPages:'{count} sidor i den lokala indexen', openData:'Öppna data', noGenerativeAi:'Ingen generativ AI' },
        da: { quickWiki:'Hurtig wiki', quickWikiExample:'Prøv "udgivelsesdato for PlayStation 5"', quickWikiHint:'Stil et spørgsmål for et kildebelagt, deterministisk svar', indexLoading:'Indlæser lokalt webindeks…', indexPages:'{count} sider i det lokale indeks', openData:'Åbne data', noGenerativeAi:'Ingen generativ AI' },
        fi: { quickWiki:'Pika-wiki', quickWikiExample:'Kokeile "PlayStation 5 julkaisupäivä"', quickWikiHint:'Kysy lähteistetty, deterministinen vastaus', indexLoading:'Ladataan paikallista verkkoindeksiä…', indexPages:'{count} sivua paikallisessa indeksissä', openData:'Avoin data', noGenerativeAi:'Ei generatiivista tekoälyä' },
        no: { quickWiki:'Hurtig wiki', quickWikiExample:'Prøv «utgivelsesdato for PlayStation 5»', quickWikiHint:'Still et spørsmål for et kilderespondert, deterministisk svar', indexLoading:'Laster lokal webindeks…', indexPages:'{count} sider i den lokale indeksen', openData:'Åpne data', noGenerativeAi:'Ingen generativ KI' },
        cs: { quickWiki:'Rychlá wiki', quickWikiExample:'Zkuste „datum vydání PlayStation 5“', quickWikiHint:'Položte otázku pro determinovanou odpověď se zdroji', indexLoading:'Načítá se místní webový index…', indexPages:'{count} stránek v místním indexu', openData:'Otevřená data', noGenerativeAi:'Bez generativní AI' },
        ro: { quickWiki:'Wiki rapid', quickWikiExample:'Încearcă „data lansării PlayStation 5”', quickWikiHint:'Pune o întrebare pentru un răspuns determinist cu surse', indexLoading:'Se încarcă indexul web local…', indexPages:'{count} pagini în indexul local', openData:'Date deschise', noGenerativeAi:'Fără AI generativ' },
        hu: { quickWiki:'Gyors wiki', quickWikiExample:'Próbáld: „mikor jelent meg a PlayStation 5?”', quickWikiHint:'Kérdezz forrásolt, determinisztikus választ', indexLoading:'A helyi webindex betöltése…', indexPages:'{count} oldal a helyi indexben', openData:'Nyílt adatok', noGenerativeAi:'Nincs generatív AI' },
        el: { quickWiki:'Γρήγορο wiki', quickWikiExample:'Δοκίμασε «ποτε κυκλοφόρησε το PlayStation 5»', quickWikiHint:'Κάνε ερώτηση για τεκμηριωμένη, ντετερμινιστική απάντηση', indexLoading:'Φόρτωση τοπικού web index…', indexPages:'{count} σελίδες στο τοπικό index', openData:'Ανοιχτά δεδομένα', noGenerativeAi:'Χωρίς παραγωγική AI' }
    };
    for (const code of Object.keys(LANG)) {
        const copy = QUICK_COPY[code] || QUICK_COPY.en;
        for (const key of Object.keys(QUICK_COPY.en)) {
            if (!LANG[code][key]) LANG[code][key] = copy[key];
        }
    }

    const LANG_LIST = [
        { code:'en', name:'English' }, { code:'es', name:'Español' }, { code:'fr', name:'Français' },
        { code:'de', name:'Deutsch' }, { code:'it', name:'Italiano' }, { code:'pt', name:'Português' },
        { code:'ru', name:'Русский' }, { code:'ja', name:'日本語' }, { code:'zh-CN', name:'简体中文' },
        { code:'zh-TW', name:'繁體中文' }, { code:'ko', name:'한국어' }, { code:'ar', name:'العربية' },
        { code:'hi', name:'हिन्दी' }, { code:'bn', name:'বাংলা' }, { code:'tr', name:'Türkçe' },
        { code:'nl', name:'Nederlands' }, { code:'pl', name:'Polski' }, { code:'sv', name:'Svenska' },
        { code:'da', name:'Dansk' }, { code:'fi', name:'Suomi' }, { code:'no', name:'Norsk' },
        { code:'cs', name:'Čeština' }, { code:'ro', name:'Română' }, { code:'hu', name:'Magyar' },
        { code:'el', name:'Ελληνικά' }
    ];

    const TZ_COUNTRY = {
        'Europe/London':'GB','Europe/Paris':'FR','Europe/Berlin':'DE','Europe/Rome':'IT',
        'Europe/Madrid':'ES','Europe/Lisbon':'PT','Europe/Moscow':'RU','Europe/Athens':'GR',
        'Europe/Amsterdam':'NL','Europe/Brussels':'BE','Europe/Stockholm':'SE',
        'Europe/Copenhagen':'DK','Europe/Oslo':'NO','Europe/Helsinki':'FI','Europe/Prague':'CZ',
        'Europe/Bucharest':'RO','Europe/Budapest':'HU','Europe/Warsaw':'PL','Europe/Vienna':'AT',
        'Europe/Zurich':'CH','Europe/Istanbul':'TR','Asia/Tokyo':'JP','Asia/Seoul':'KR',
        'Asia/Shanghai':'CN','Asia/Taipei':'TW','Asia/Hong_Kong':'HK','Asia/Kolkata':'IN',
        'Asia/Dhaka':'BN','Asia/Bangkok':'TH','Asia/Singapore':'SG','Asia/Dubai':'AE',
        'Asia/Jerusalem':'IL','Asia/Riyadh':'SA','Asia/Tehran':'IR','Asia/Baghdad':'IQ',
        'America/New_York':'US','America/Chicago':'US','America/Denver':'US','America/Los_Angeles':'US',
        'America/Toronto':'CA','America/Vancouver':'CA','America/Mexico_City':'MX',
        'America/Sao_Paulo':'BR','America/Argentina/Buenos_Aires':'AR',
        'America/Bogota':'CO','America/Santiago':'CL','America/Lima':'PE',
        'Australia/Sydney':'AU','Australia/Melbourne':'AU','Pacific/Auckland':'NZ',
        'Africa/Cairo':'EG','Africa/Johannesburg':'ZA','Africa/Lagos':'NG','Africa/Nairobi':'KE'
    };

    function detectCountry() {
        try {
            const tz = Intl.DateTimeFormat().resolvedOptions().timeZone;
            return TZ_COUNTRY[tz] || 'GB';
        } catch(_) { return 'GB'; }
    }

    function countryName(code, lang) {
        try { return new Intl.DisplayNames([lang], { type: 'region' }).of(code); }
        catch(_) { return code; }
    }

    function detectBrowserLang() {
        try {
            const raw = String(navigator.language || navigator.userLanguage || 'en');
            if (LANG[raw]) return raw;
            const lower = raw.toLowerCase();
            if (lower.startsWith('zh')) {
                return /(?:tw|hk|mo|hant)/i.test(lower) ? 'zh-TW' : 'zh-CN';
            }
            const base = lower.split('-')[0];
            return LANG[base] ? base : 'en';
        } catch(_) { return 'en'; }
    }

    let _langPreference = localStorage.getItem('gw-lang') || 'auto';
    if (_langPreference !== 'auto' && !LANG[_langPreference]) _langPreference = 'auto';
    let _lang = _langPreference === 'auto' ? detectBrowserLang() : _langPreference;
    let localIndexCount = 0;

    function _t(key, vars) {
        let s = (LANG[_lang] && LANG[_lang][key]) || LANG.en[key] || key;
        if (vars) for (const k in vars) s = s.split('{'+k+'}').join(vars[k]);
        return s;
    }

    function updateCountry() {
        const code = detectCountry();
        const displayLang = _langPreference === 'auto' ? detectBrowserLang() : _langPreference;
        const name = countryName(code, displayLang);
        document.querySelectorAll('[data-i18n-country]').forEach(el => el.textContent = name);
    }

    function _applyTranslations() {
        document.documentElement.lang = _lang || 'en';
        document.documentElement.dir = /^ar\b/i.test(_lang || '') ? 'rtl' : 'ltr';
        document.querySelectorAll('[data-i18n]').forEach(el => {
            const key = el.getAttribute('data-i18n');
            if (el.tagName === 'INPUT' || el.tagName === 'TEXTAREA') return;
            el.textContent = _t(key);
        });
        document.querySelectorAll('[data-i18n-placeholder]').forEach(el => {
            el.placeholder = _t(el.getAttribute('data-i18n-placeholder'));
        });
        document.title = 'Gateway';
        const sel = $('langSelect');
        if (sel) sel.value = _langPreference;
        const dt = $('settingsDarkToggle');
        if (dt) dt.checked = document.body.classList.contains('dark');
        _syncQuickWikiToggle();
        const indexStats = $('indexStats');
        if (indexStats) {
            indexStats.textContent = localIndexCount
                ? _t('indexPages', { count: localIndexCount.toLocaleString(_lang) })
                : _t('indexLoading');
        }
    }

    window.setLanguage = function(lang) {
        if (lang !== 'auto' && !LANG[lang]) return;
        const previousLang = _lang;
        const previousPreference = _langPreference;
        _langPreference = lang;
        localStorage.setItem('gw-lang', lang);
        _lang = lang === 'auto' ? detectBrowserLang() : lang;
        window.gatewayLang = _lang;
        _applyTranslations();
        updateCountry();
        const sel = $('langSelect');
        if (sel) sel.value = _langPreference;
        // A language change also changes the Wikipedia/Wikidata endpoint and
        // cache namespace. Invalidate an in-flight response and refresh a
        // submitted search so old-language data cannot be shown as current.
        if (lastQuery && $('resultsUI') && $('resultsUI').classList.contains('visible')
            && (_lang !== previousLang || _langPreference !== previousPreference)) {
            performSearch('resultsSearchInput');
        }
    };
    window.gatewayLang = _lang;

    // ======================== SETTINGS ========================

    window.openSettings = function(e) {
        const overlay = $('settingsOverlay');
        if (overlay) overlay.classList.add('show');
        const dt = $('settingsDarkToggle');
        if (dt) dt.checked = document.body.classList.contains('dark');
        _syncQuickWikiToggle();
        const sel = $('langSelect');
        if (sel) sel.value = _langPreference;
    };

    window.closeSettings = function(e) {
        if (e && e.target !== e.currentTarget) return;
        const overlay = $('settingsOverlay');
        if (overlay) overlay.classList.remove('show');
    };

    window.handleIndexUpload = function(e) {
        const file = e.target.files[0];
        if (!file) return;
        const reader = new FileReader();
        reader.onload = function(ev) {
            try {
                const data = JSON.parse(ev.target.result);
                if (Array.isArray(data) && data.length) {
                    window.gatewaySetIndex(data);
                    closeSettings();
                }
            } catch(_) {}
        };
        reader.readAsText(file);
    };

    // ======================== STATE ========================

    let allResults = [];
    const PER_PAGE = 20;
    let quickWikiResult = null;
    let spellSuggestion = null;
    let lastQuery = '';
    let searchRequestId = 0;
    window._gwPage = 1;
    window._gwLoaded = 0;

    // ======================== DARK MODE ========================

    function applyDark(enabled) {
        document.documentElement.classList.toggle('dark', enabled);
        document.body.classList.toggle('dark', enabled);
        localStorage.setItem('gw-dark', enabled ? '1' : '0');
        document.querySelectorAll('.sun').forEach(el => el.style.display = enabled ? 'none' : '');
        document.querySelectorAll('.moon').forEach(el => el.style.display = enabled ? '' : 'none');
        const dt = $('settingsDarkToggle');
        if (dt) dt.checked = enabled;
    }
    window.toggleDarkMode = function() {
        applyDark(!document.documentElement.classList.contains('dark'));
    };
    if (localStorage.getItem('gw-dark') === '1') applyDark(true);

    // ======================== QUICK WIKI SETTING ========================

    let _quickWikiEnabled = true;
    try { _quickWikiEnabled = localStorage.getItem('gw-quickwiki') !== '0'; }
    catch(_) { _quickWikiEnabled = true; }

    function _syncQuickWikiToggle() {
        const t = $('settingsQuickWikiToggle');
        if (t) t.checked = _quickWikiEnabled;
    }

    function applyQuickWiki(enabled) {
        _quickWikiEnabled = !!enabled;
        try { localStorage.setItem('gw-quickwiki', _quickWikiEnabled ? '1' : '0'); }
        catch(_) {}
        _syncQuickWikiToggle();
        if (!_quickWikiEnabled) {
            // Hide the box. Stale in-flight responses are neutralized by the
            // enabled-guards in renderQuickWiki and the fetch handler below,
            // so the main results and web enrichment are unaffected.
            quickWikiResult = null;
            const area = $('answerArea');
            if (area) area.innerHTML = '';
        } else if (lastQuery && $('resultsUI') && $('resultsUI').classList.contains('visible')) {
            // Re-enable: lazily fetch the answer for the current query only.
            const requestId = searchRequestId;
            const q = lastQuery;
            if (typeof window.gatewayQuickWiki === 'function') {
                window.gatewayQuickWiki(q).then(wikiAnswer => {
                    if (requestId !== searchRequestId) return;
                    if (!wikiAnswer || !wikiAnswer.answer) return;
                    quickWikiResult = wikiAnswer;
                    renderQuickWiki();
                }).catch(() => {});
            }
        }
    }
    window.toggleQuickWiki = function() {
        applyQuickWiki(!_quickWikiEnabled);
    };
    window.setQuickWikiEnabled = applyQuickWiki;
    window.gatewayQuickWikiEnabled = function() { return _quickWikiEnabled; };

    // ======================== CLEAR BUTTON ========================

    window.clearSearch = function(inputId) {
        const input = $(inputId);
        if (input) { input.value = ''; input.focus(); }
        searchRequestId++;
        updateClearBtn(inputId);
    };
    function updateClearBtn(inputId) {
        const input = $(inputId);
        const btn = inputId === 'mainSearchInput' ? $('homeClearBtn') : $('resultsClearBtn');
        if (input && btn) btn.classList.toggle('visible', input.value.length > 0);
    }

    // ======================== AUTOCOMPLETE ========================

    const autoState = {};

    function initAutocomplete(inputId, dropdownId) {
        const input = $(inputId);
        const dd = $(dropdownId);
        if (!input || !dd) return;
        const wrapper = input.closest('.search-bar-wrapper');
        let timer, sug = [], hl = -1, open = false, suggestionRequest = 0;

        function close() {
            suggestionRequest++;
            dd.classList.remove('show');
            if (wrapper) wrapper.classList.remove('dd-open');
            open = false; hl = -1;
        }
        function highlight(idx) {
            const items = dd.querySelectorAll('.autocomplete-item');
            items.forEach((el, i) => el.classList.toggle('highlighted', i === idx));
            hl = idx;
            if (idx >= 0 && items[idx]) items[idx].scrollIntoView({ block: 'nearest' });
        }
        function select(idx) {
            if (idx < 0 || idx >= sug.length) return;
            input.value = sug[idx]; close(); updateClearBtn(inputId); doSearch(inputId);
        }
        function render(list) {
            sug = (Array.isArray(list) ? list : []).filter(item => typeof item === 'string' && item.length).slice(0, 8);
            dd.innerHTML = '';
            if (!sug.length) { close(); return; }
            sug.forEach((s, i) => {
                const div = document.createElement('div');
                div.className = 'autocomplete-item';
                const idx = s.toLowerCase().indexOf(input.value.toLowerCase());
                const icon = document.createElement('span');
                icon.className = 'autocomplete-icon';
                icon.textContent = '⌕';
                div.appendChild(icon);
                if (idx >= 0) {
                    div.appendChild(document.createTextNode(s.slice(0, idx)));
                    const match = document.createElement('span');
                    match.className = 'match';
                    match.textContent = s.slice(idx, idx + input.value.length);
                    div.appendChild(match);
                    div.appendChild(document.createTextNode(s.slice(idx + input.value.length)));
                } else {
                    div.appendChild(document.createTextNode(s));
                }
                div.onclick = () => { input.value = s; close(); updateClearBtn(inputId); doSearch(inputId); };
                div.onmouseenter = () => highlight(i);
                dd.appendChild(div);
            });
            dd.classList.add('show');
            if (wrapper) wrapper.classList.add('dd-open');
            open = true; hl = -1;
        }

        input.addEventListener('input', function() {
            clearTimeout(timer);
            const val = this.value.trim();
            const token = ++suggestionRequest;
            if (val.length < 2) { close(); return; }
            timer = setTimeout(async () => {
                if (typeof getSuggestions !== 'undefined') {
                    const sugs = await getSuggestions(val);
                    if (token === suggestionRequest && this.value.trim() === val) render(sugs);
                }
            }, 250);
        });
        input.addEventListener('keydown', function(e) {
            if (!open) return;
            if (e.key === 'ArrowDown') { e.preventDefault(); highlight(hl < sug.length - 1 ? hl + 1 : 0); }
            else if (e.key === 'ArrowUp') { e.preventDefault(); highlight(hl > 0 ? hl - 1 : sug.length - 1); }
            else if (e.key === 'Enter' && hl >= 0) { e.preventDefault(); select(hl); }
            else if (e.key === 'Escape') close();
        });
        input.addEventListener('blur', () => setTimeout(close, 150));
        input.addEventListener('focus', function() {
            if (sug.length && this.value.trim().length >= 2) {
                dd.classList.add('show');
                if (wrapper) wrapper.classList.add('dd-open');
                open = true;
            }
        });
        autoState[inputId] = { close };
    }

    function doSearch(inputId) {
        const input = $(inputId);
        if (!input || !input.value.trim()) return;
        if (autoState[inputId]) autoState[inputId].close();
        performSearch(inputId);
    }

    // ======================== XPDEV PROFILE ========================
    // Searching "XPDevs" (or anything that mentions it) is answered from a
    // local, deterministic profile: the Quick Wiki box with the XPDevs logo,
    // the official website, the GitHub organisation, and every public
    // repository it publishes. No network round-trip, no generative AI.

    const XPDEV_PROFILE = {
        name: 'XPDevs',
        website: 'https://xpdevs.github.io',
        github: 'https://github.com/XPDevs',
        logo: 'https://xpdevs.github.io/logo/XPDevs.png',
        repoCount: 59,
        location: 'United Kingdom',
        focus: 'operating systems, web tools and open-source experiments'
    };

    // [name, language, description, stars, updated, fork?]
    const XPDEV_REPO_DATA = [
        ['64232', 'JavaScript', 'Convert 64 bit exes to 32 bit exes', 1, '2025-08-15'],
        ['A-bad-codebase-scanner', 'C', 'This was supposed to be working but i kinda doesnt if you want to modify it you can freely', 0, '2026-06-20'],
        ['aimi', '', 'The langauge of C but giving the dev more control', 0, '2026-06-26'],
        ['ASM-plus-', 'C', 'this repo contains the code for the "compiler" for a custom assmelby language made by XPDevs for use in booloaders, it takes the cusomt ASM and turns ', 0, '2025-10-01'],
        ['AudioNoise', '', 'Random digital audio effects', 0, '2026-01-11', true],
        ['AuraOS', 'HTML', 'This repo contains the code for the AuraOS interface Link: https://xpdevs.github.io/AuraOS', 0, '2026-09-15'],
        ['Beluma', 'HTML', 'https://xpdevs.github.io/Beluma', 0, '2026-09-16'],
        ['CastOS', 'HTML', 'an operating system for casting so you dont have to pay for a chromecast', 1, '2025-02-15'],
        ['chess', 'HTML', 'A very bad chess website', 0, '2026-06-08'],
        ['CMD', 'HTML', 'No repository description yet.', 0, '2026-09-23'],
        ['controller.API', 'HTML', 'a free and open source API to give any website controller support', 0, '2025-10-27'],
        ['copyright', '', 'the javascritp code for the XPDevs copyright footer', 0, '2025-02-17'],
        ['Doors-Operating-System', 'HTML', 'This is the old wesite for the XPDevs website', 0, '2026-04-29'],
        ['DoorsOS-Server', 'HTML', 'DoorsOS Server OS', 0, '2025-02-16'],
        ['exe2msi', '', 'No repository description yet.', 0, '2025-03-03'],
        ['Foresight', 'HTML', 'No repository description yet.', 0, '2026-02-26'],
        ['gateway', 'JavaScript', 'A small search engine made from scratch', 1, '2026-09-27'],
        ['Genesis-AI', 'JavaScript', 'This is the official website for the Genisis AI made by XPDevs', 0, '2026-07-15'],
        ['Genesis-pof', 'Python', 'simple proof of concept for a new Genesis model', 0, '2026-08-19'],
        ['Genvid', 'JavaScript', 'TEST', 0, '2026-05-08'],
        ['ggwave', '', 'Tiny data-over-sound library', 0, '2025-10-18', true],
        ['Hostly', 'HTML', 'Host any website for free', 0, '2026-03-19'],
        ['HTML-APP-TEMPLATES', 'HTML', 'No repository description yet.', 0, '2025-05-19'],
        ['Hue', 'HTML', 'An app that makes small images of art based on your moods for that week based on the colours you pick', 0, '2026-04-08'],
        ['iframe-control', 'JavaScript', 'this basic JS api allows you to call the api and show an iframe and pass control to it.', 0, '2025-10-02'],
        ['JS2PY', 'HTML', 'A program that allows you to convert Javascript to Python', 0, '2025-11-17'],
        ['L', '', 'Virtual Machine for the Web', 0, '2025-02-15', true],
        ['Learn-Munhwao', 'JavaScript', 'An app for learning the north Korean language (Munhwao)', 0, '2026-09-25'],
        ['Linux', 'HTML', 'No repository description yet.', 0, '2025-10-02'],
        ['LM-Studio-Image-Search', 'TypeScript', 'A simple MCP server that allows models trained for tool use in LM Studio to search the web for images and display them.', 0, '2026-06-30'],
        ['MacOS', 'HTML', 'MacOS in html, if you would like it to be taken down please visit https://xpdevs,github.io', 0, '2026-09-09'],
        ['Messaging', 'HTML', 'No repository description yet.', 0, '2026-05-06'],
        ['Mobile-html-app-tool', 'HTML', 'Mobile html app tool', 1, '2025-10-01'],
        ['MONS', 'HTML', 'No repository description yet.', 1, '2026-01-16'],
        ['Musicify', 'HTML', 'This website applies diffrent effects onto the mp3 file you upload', 0, '2025-09-15'],
        ['NetShield', 'JavaScript', 'This is a very  bad but semi-functional adblocker', 0, '2026-06-19'],
        ['NexShell', 'C', 'NexShell source code', 1, '2026-07-29'],
        ['OFL', 'JavaScript', 'A new fast search standard for databases', 0, '2026-08-25'],
        ['Paral', '', 'Javascript 3D library built upon THREE.js', 0, '2026-02-19'],
        ['PS4', 'JavaScript', 'No repository description yet.', 0, '2025-04-04'],
        ['RetroBoot', 'C', 'RetroBoot allows BIOS based OS to run on UEFI based PCs only without modiciation to anything', 0, '2026-03-12'],
        ['stein-controller', 'JavaScript', 'this js file gives stein.world support for controllers as it currently does not', 0, '2025-04-10'],
        ['telcom', 'C', 'An open-source framework for managing packet queues, latency on access network gear.', 1, '2026-07-19'],
        ['The-Holy-Cheddar-Faith', 'HTML', 'This is the website for The Holy Cheddar Religion', 0, '2025-12-10'],
        ['Undertale', 'JavaScript', 'This is a working in progress to make the entire UNDERTALE game in pure html css and js', 0, '2026-03-30'],
        ['unlocking-fire-tablet-bootloader', '', 'No repository description yet.', 0, '2024-12-15'],
        ['URL', 'HTML', 'shorterns urls instead of using Tiny URL', 0, '2026-02-26'],
        ['VirtualDoors', 'HTML', 'DoorsOS vm running in browser', 0, '2026-01-20', true],
        ['WebVM', 'HTML', 'collection of vms for a variety of devices all in the web browser', 9, '2026-06-14', true],
        ['Westgress-Sports', 'HTML', 'No repository description yet.', 1, '2025-02-15'],
        ['WhiteSur', '', 'No repository description yet.', 0, '2025-04-10'],
        ['Windows10', 'HTML', 'Windows 10 in the Web', 1, '2026-03-31'],
        ['WiseAssist', 'HTML', 'The repo contains the html code for the android app called WiseAssist to help elders with their phones it also containslink to the apk file hosted in ', 0, '2026-02-28'],
        ['XJS', 'JavaScript', 'XJS is the custom XPDevs version of normal JS allowing to be easly inter-changable', 0, '2026-02-01'],
        ['xmrigC', '', 'RandomX, KawPow, CryptoNight and GhostRider unified CPU/GPU miner and RandomX benchmark Recoded all into C', 0, '2026-01-11', true],
        ['XPDevs', '', 'No repository description yet.', 0, '2026-06-28'],
        ['XPDevs-Studio', 'JavaScript', 'A small very new and unfinished Linux IDE.', 0, '2026-08-30'],
        ['xpdevs.github.io', 'HTML', 'The official XPDevs Website', 1, '2026-09-27'],
        ['Youtube-On-Kodi', 'Python', 'No repository description yet.', 1, '2026-08-22'],
    ];

    function _isXpdevsQuery(query) {
        const q = String(query || '').toLowerCase();
        return /\bxp[ _-]?devs?\b/.test(q) || /\bgateway search\b/.test(q);
    }

    function _xpdevsResult(title, url, description, sourceLabel, score) {
        return {
            title: title,
            url: url,
            description: description,
            fullSnippet: description,
            extract: description,
            thumbnail: null,
            source: 'special',
            sourceLabel: sourceLabel,
            resultType: 'web',
            score: score,
            domain: _displayDomain(url),
            suggestion: null
        };
    }

    function _xpdevsRepoResults() {
        return XPDEV_REPO_DATA.map((row, i) => {
            const name = row[0], lang = row[1], desc = row[2], stars = row[3], updated = row[4];
            const fork = row[5] ? ' \u00b7 fork of an upstream project' : '';
            const meta = [lang, '\u2605 ' + stars, 'updated ' + updated + fork].filter(Boolean).join(' \u00b7 ');
            return _xpdevsResult(
                'XPDevs / ' + name,
                XPDEV_PROFILE.github + '/' + name,
                desc + ' \u2014 ' + meta,
                'GitHub',
                90000 - i
            );
        });
    }

    const XPDEV_SITES = [
        _xpdevsResult(
            'XPDevs',
            XPDEV_PROFILE.website,
            'The official XPDevs website: ' + XPDEV_PROFILE.focus + '. Based in the '
                + XPDEV_PROFILE.location + '. Home of DoorsOS, ExamOS and Genesis-AI.',
            'XPDevs',
            99999
        ),
        _xpdevsResult(
            'XPDevs on GitHub',
            XPDEV_PROFILE.github,
            'Every public repository published by XPDevs \u2014 ' + XPDEV_PROFILE.repoCount
                + ' projects in total, from Gateway and AuraOS to WebVM, Hostly and XJS.',
            'GitHub',
            99998
        ),
        _xpdevsResult(
            'Gateway Search',
            'https://xpdevs.github.io/gateway',
            'Gateway, the multi-source search engine by XPDevs. Fast, private, no API keys needed.',
            'Gateway',
            99997
        )
    ].concat(_xpdevsRepoResults());

    function _boostXpdevs(results, query) {
        if (_isXpdevsQuery(query)) {
            const existing = new Set(results.map(r => r.url));
            const toAdd = XPDEV_SITES.filter(s => !existing.has(s.url));
            // Pinned ahead of every ranked result, so the profile, the site
            // and the full repository list are the first rows of the page.
            results.unshift(...toAdd);
        }
        // gatewayCrawl caps its own ranked set at 100; keep the same contract
        // after adding the XPDevs shortcut set (its 62 rows sit inside it).
        return results.slice(0, 100);
    }

    // Quick Wiki for XPDevs queries: deterministic, local, logo-first.
    function _xpdevsQuickWiki(query) {
        if (!_isXpdevsQuery(query)) return null;
        const p = XPDEV_PROFILE;
        return {
            answer: 'XPDevs is an open-source developer based in the ' + p.location
                + ' that builds ' + p.focus + '. The project publishes ' + p.repoCount
                + ' public repositories on GitHub and is best known for Gateway, a small '
                + 'multi-source search engine written from scratch, plus the DoorsOS and '
                + 'ExamOS operating-system experiments and the Genesis-AI platform.',
            description: 'The current focus is Genesis-AI and Genesis AI Studio, a local AI '
                + 'experience; DoorsOS is the flagship operating system, built from the kernel up.',
            title: p.name,
            url: p.website,
            label: 'Project profile',
            sourceLabel: p.name,
            property: p.repoCount + ' public repositories',
            note: 'Local profile \u00b7 github.com/XPDevs',
            image: p.logo,
            imageAlt: p.name + ' logo',
            imageCaption: p.name + ' logo',
            sourceLinks: [
                { url: p.github, title: 'XPDevs on GitHub', domain: 'github.com' },
                { url: 'https://xpdevs.github.io/Doors', title: 'DoorsOS', domain: 'xpdevs.github.io' },
                { url: 'https://xpdevs.github.io/ExamOS', title: 'ExamOS', domain: 'xpdevs.github.io' },
                { url: 'https://xpdevs.github.io/AI', title: 'Genesis-AI', domain: 'xpdevs.github.io' }
            ]
        };
    }

    // ======================== SEARCH ========================

    function _quickWikiSkeleton() {
        return `
            <div class="answer-box quick-wiki-loading" aria-live="polite" aria-busy="true">
                <div class="spinner"></div>
                <div><strong>${_t('quickWiki')}</strong><span>${_t('quickWikiHint')}</span></div>
            </div>`;
    }

    async function performSearch(inputId) {
        const input = $(inputId);
        if (!input) return false;
        const q = input.value.trim();
        if (!q) return false;

        const requestId = ++searchRequestId;
        const t0 = (typeof performance !== 'undefined' && performance.now) ? performance.now() : Date.now();
        lastQuery = q;
        spellSuggestion = null;
        quickWikiResult = null;
        window._gwSearchMs = null;

        if ($('mainSearchInput')) $('mainSearchInput').value = q;
        if ($('resultsSearchInput')) $('resultsSearchInput').value = q;
        updateClearBtn('mainSearchInput');
        updateClearBtn('resultsSearchInput');

        if (autoState.mainSearchInput) autoState.mainSearchInput.close();
        if (autoState.resultsSearchInput) autoState.resultsSearchInput.close();

        if ($('homeUI')) $('homeUI').style.display = 'none';
        if ($('resultsUI')) $('resultsUI').classList.add('visible');

        if ($('didYouMean')) $('didYouMean').innerHTML = '';
        if ($('emptyState')) $('emptyState').style.display = 'none';
        // Quick Wiki is lazy: show a lightweight skeleton now, replace it
        // asynchronously after the main results have already painted.
        // Skipped entirely when Quick Wiki is disabled in Settings.
        if ($('answerArea')) $('answerArea').innerHTML = _quickWikiEnabled ? _quickWikiSkeleton() : '';
        if ($('resultsList')) {
            $('resultsList').innerHTML = `
                <div class="loading-state">
                    <div class="spinner"></div>
                    <p>${_t('searchingMultipleSources')}</p>
                </div>`;
        }
        if ($('resultStats')) $('resultStats').textContent = '';
        if ($('loadMoreArea')) $('loadMoreArea').style.display = 'none';

        try {
            const url = new URL(window.location);
            url.searchParams.set('query', q);
            window.history.replaceState({ query: q }, '', url.toString());
        } catch(_) {}

        window._gwPage = 1;
        window._gwLoaded = 0;

        // ---- STAGE 1 (fast): local index only, no network. ----
        // This is what makes Gateway faster than Google for the main list:
        // the first paint never waits for Wikipedia/Wikidata round-trips.
        let localResults = [];
        try {
            if (typeof window.gatewaySearchLocal === 'function') {
                localResults = await window.gatewaySearchLocal(q);
            } else {
                localResults = await window.gatewayCrawl(q);
            }
        } catch(_) { localResults = []; }
        if (requestId !== searchRequestId) return false;

        allResults = _boostXpdevs(localResults || [], q);
        const paint = () => {
            if (requestId !== searchRequestId) return;
            const t1 = (typeof performance !== 'undefined' && performance.now) ? performance.now() : Date.now();
            window._gwSearchMs = Math.max(1, Math.round(t1 - t0));
            window._gwLoaded = 0;
            window._gwRender();
        };
        if (typeof requestAnimationFrame === 'function') {
            await new Promise(resolve => requestAnimationFrame(() => { paint(); resolve(); }));
        } else {
            paint();
        }
        if (requestId !== searchRequestId) return false;

        // ---- STAGE 2 (async enrichment): full crawl + Quick Wiki + spell. ----
        // Each resolves independently and re-renders only its own section,
        // so a slow network never blocks or replaces the fast main list
        // with a spinner.
        if (typeof window.gatewayCrawl === 'function') {
            window.gatewayCrawl(q).then(full => {
                if (requestId !== searchRequestId || !Array.isArray(full) || !full.length) return;
                const seen = new Set(allResults.map(r => r.url));
                let added = 0;
                for (const r of _boostXpdevs(full, q)) {
                    if (!seen.has(r.url)) { seen.add(r.url); allResults.push(r); added++; }
                }
                if (added > 0) {
                    allResults.sort((a, b) => (b.score || 0) - (a.score || 0));
                    // Preserve already-rendered items; only extend on "show more".
                    if ($('resultStats')) {
                        const secs = window._gwSearchMs != null ? (window._gwSearchMs / 1000).toFixed(2) : null;
                        $('resultStats').textContent = _t('resultsStats', { count: allResults.length })
                            + (secs != null ? ` (${secs} seconds)` : '');
                    }
                }
            }).catch(() => {});
        }

        // XPDevs queries are answered locally (logo + profile + repo list) and
        // never wait on Wikipedia, which has no article for the project.
        const xpdevsAnswer = _xpdevsQuickWiki(q);
        if (xpdevsAnswer) {
            quickWikiResult = xpdevsAnswer;
            renderQuickWiki();
        } else if (_quickWikiEnabled && typeof window.gatewayQuickWiki === 'function') {
            window.gatewayQuickWiki(q).then(wikiAnswer => {
                if (requestId !== searchRequestId || !_quickWikiEnabled) return;
                // Wikipedia could not answer: hide the Quick Wiki box
                // entirely (clear the skeleton) rather than showing a weak
                // or empty answer.
                if (!wikiAnswer || !wikiAnswer.answer) {
                    quickWikiResult = null;
                    const area = $('answerArea');
                    if (area) area.innerHTML = '';
                    return;
                }
                quickWikiResult = wikiAnswer;
                renderQuickWiki();
            }).catch(() => {
                if (requestId !== searchRequestId) return;
                // No Quick Wiki match: remove the skeleton so it never
                // blocks the already-visible main results.
                const area = $('answerArea');
                if (area && area.querySelector('.quick-wiki-loading')) area.innerHTML = '';
            });
        } else if ($('answerArea')) {
            $('answerArea').innerHTML = '';
        }

        if (typeof window.gatewaySpellCheck === 'function') {
            window.gatewaySpellCheck(q).then(suggestion => {
                if (requestId !== searchRequestId) return;
                if (suggestion && suggestion.toLowerCase() !== q.toLowerCase()) {
                    spellSuggestion = suggestion;
                    renderDidYouMean();
                }
            }).catch(() => {});
        }

        return true;
    }
    window.performSearch = performSearch;

    // ======================== RENDER ========================

    function _escapeHtml(value) {
        return String(value == null ? '' : value).replace(/[&<>'"]/g, character => ({
            '&': '&amp;', '<': '&lt;', '>': '&gt;', "'": '&#39;', '"': '&quot;'
        })[character]);
    }

    function _safeHref(value) {
        try {
            const url = new URL(value);
            if (!['http:', 'https:'].includes(url.protocol) || url.username || url.password) return '#';
            const host = url.hostname.toLowerCase().replace(/^\[|\]$/g, '').replace(/\.$/, '');
            const dangerous = new Set([
                '4chan.org', '8kun.top', 'bestgore.com', 'liveleak.com', 'kiwifarms.net',
                'freenode.net', 'anon-ib.com', 'bitcoinmix.org', 'hydramarket.org',
                'tor2web.org', 'onion.city', 'onion.to', 'onion.cab', 'onion.sh',
                'onion.link', 'onion.guide', 'exe.io', 'shorte.st', 'adf.ly',
                'bit.ly', 'tinyurl.com', 'ow.ly', 'goo.gl', 'is.gd', 'buff.ly',
                'tiny.cc', 'tr.im', 'x.co', 'short.cm'
            ]);
            if (!host || dangerous.has(host.replace(/^www\./, ''))
                || host === 'localhost' || host.endsWith('.local') || host.endsWith('.internal')
                || host === '::1' || host === '::' || host.includes(':')
                || /^(?:0\.0\.0\.0|127(?:\.\d{1,3}){3}|10(?:\.\d{1,3}){3}|169\.254(?:\.\d{1,3}){2}|192\.168(?:\.\d{1,3}){2}|172\.(?:1[6-9]|2\d|3[0-1])(?:\.\d{1,3}){2})$/i.test(host)) {
                return '#';
            }
            return url.href;
        } catch(_) { return '#'; }
    }

    function _displayDomain(value) {
        try {
            return new URL(value).hostname.toLowerCase().replace(/^www\./, '').replace(/\.$/, '');
        } catch(_) { return ''; }
    }

    function _fmtUrl(u) {
        if (!u) return '';
        var clean = String(u).replace(/^https?:\/\//, '').replace(/\/$/, '');
        var parts = clean.split('/');
        if (parts.length > 1) {
            return parts[0] + ' \u203A ' + parts.slice(1).join(' \u203A ');
        }
        return clean;
    }

    function _faviconHtml(domain, size) {
        // Every web URL shows its favicon: Google S2 primary, DDG fallback,
        // letter avatar as the final fallback so nothing renders icon-less.
        const d = _escapeHtml(domain || '');
        if (!d) return '';
        const px = size || 16;
        const primary = 'https://www.google.com/s2/favicons?domain=' + encodeURIComponent(domain) + '&sz=32';
        const fallback = 'https://icons.duckduckgo.com/ip3/' + encodeURIComponent(domain) + '.ico';
        const letter = (domain || '?').trim().charAt(0).toUpperCase();
        return `<span class="result-favicon-wrap" aria-hidden="true">`
            + `<img class="result-favicon" width="${px}" height="${px}" alt="" loading="lazy" decoding="async" `
            + `src="${_escapeHtml(primary)}" `
            + `onerror="if(!this.dataset.fb){this.dataset.fb='1';this.src='${_escapeHtml(fallback)}';}`
            + `else{this.style.display='none';var p=this.parentElement;if(p)p.classList.add('favicon-fallback');`
            + `if(p&&!p.querySelector('.favicon-letter')){var s=document.createElement('span');s.className='favicon-letter';s.textContent='${_escapeHtml(letter)}';p.appendChild(s);}}" />`
            + `</span>`;
    }

    function _answerLinkHtml(link) {
        const safe = _safeHref(link.url);
        const domain = _displayDomain(safe) || link.domain || '';
        return `<span class="answer-link-wrap">${_faviconHtml(domain, 14)}`
            + `<a href="${_escapeHtml(safe)}" target="_self" class="answer-link">${_escapeHtml(link.domain || link.title || link.url)}</a></span>`;
    }

    function renderItem(r) {
        const url = _safeHref(r.url);
        const urlDisplay = _escapeHtml(_fmtUrl(r.url));
        const domain = _displayDomain(r.url || '') || r.domain || '';
        const source = r.sourceLabel || (r.resultType === 'wiki' ? _t('wikipedia') : 'Open web');
        return `
            <div class="result-item">
                <div class="result-item-url">
                    ${_faviconHtml(domain, 16)}
                    <span>${urlDisplay}</span>
                    <span class="result-source">${_escapeHtml(source)}</span>
                </div>
                <a class="result-item-title" href="${_escapeHtml(url)}" target="_self">${_escapeHtml(r.title)}</a>
                <div class="result-item-desc">${_escapeHtml(r.description || '')}</div>
            </div>`;
    }

    function renderDidYouMean() {
        const dym = $('didYouMean');
        if (!dym) return;
        if (spellSuggestion) {
            const queryUrl = '?query=' + encodeURIComponent(spellSuggestion);
            dym.innerHTML = `${_t('didYouMean')} <a href="${_escapeHtml(queryUrl)}">${_escapeHtml(spellSuggestion)}</a>`;
        } else dym.innerHTML = '';
    }

    function renderQuickWiki() {
        // Lazy: called only after the main results have painted, and again
        // when the async Quick Wiki fetch resolves. Never blocks Stage 1.
        // While pending (quickWikiResult === null and skeleton visible) the
        // skeleton is left untouched so the layout does not flash.
        const answerArea = $('answerArea');
        if (!answerArea) return;
        if (!_quickWikiEnabled) {
            quickWikiResult = null;
            answerArea.innerHTML = '';
            return;
        }
        const quick = quickWikiResult;
        if (!quick || !quick.answer) return;
        const quickLinks = (quick.sourceLinks || []).slice(0, 6);
        const resultLinks = allResults
            .filter(r => r.resultType === 'web' && !/wikipedia\.org|wikidata\.org/i.test(r.domain || r.url || ''))
            .slice(0, 6)
            .map(r => ({ url: r.url, title: r.domain || r.title, domain: r.domain }));
        const seenLinkUrls = new Set();
        const seenLinkDomains = new Set();
        const links = quickLinks.concat(resultLinks).filter(link => {
            if (!link || !link.url) return false;
            const safeUrl = _safeHref(link.url);
            if (safeUrl === '#') return false;
            const domain = _displayDomain(safeUrl);
            if (seenLinkUrls.has(safeUrl) || (domain && seenLinkDomains.has(domain))) return false;
            seenLinkUrls.add(safeUrl);
            if (domain) seenLinkDomains.add(domain);
            return true;
        }).slice(0, 4);
        const linksHtml = links.map(_answerLinkHtml).join('<span class="answer-sep">·</span>');
        // Defense-in-depth against duplicate answer/context text (e.g. stale
        // caches): strip any context that repeats the answer verbatim.
        let contextText = String(quick.description || '').replace(/\s+/g, ' ').trim();
        const answerNorm = String(quick.answer || '').replace(/\s+/g, ' ').trim().replace(/…\s*$/, '');
        if (contextText && answerNorm) {
            if (contextText === quick.answer || contextText === answerNorm
                || contextText.startsWith(quick.answer) || contextText.startsWith(answerNorm)
                || contextText.toLowerCase().startsWith(answerNorm.toLowerCase())) {
                const cut = contextText.startsWith(quick.answer)
                    ? contextText.slice(quick.answer.length)
                    : contextText.slice(answerNorm.length);
                contextText = cut.replace(/^[\s,;:.—–-]+/, '').trim();
                if (contextText.length < 40) contextText = '';
            }
        }
        const description = contextText
            ? `<p class="quick-wiki-context">${_escapeHtml(contextText)}</p>` : '';
        const property = quick.property
            ? `<span class="answer-property">${_escapeHtml(quick.property)}</span><span class="answer-sep">·</span>` : '';
        const imageHtml = quick.image
            ? `<figure class="quick-wiki-figure">`
              + `<img class="quick-wiki-image" src="${_escapeHtml(quick.image)}" `
              + `alt="${_escapeHtml(quick.imageAlt || ('Image for ' + quick.title))}" `
              + `loading="lazy" decoding="async" fetchpriority="low" `
              + `onerror="this.closest('.quick-wiki-figure').style.display='none'" />`
              + `<figcaption class="quick-wiki-caption">${_escapeHtml(quick.imageCaption || (quick.title + ' · Wikipedia'))}</figcaption>`
              + `</figure>`
            : '';
        // Wikipedia answers are badged "Wikipedia"; a local profile (XPDevs)
        // carries its own source label so the box never claims a false source.
        const sourceBadge = quick.sourceLabel || _t('wikipedia');

        answerArea.innerHTML = `
            <div class="answer-box quick-wiki-box">
                <div class="quick-wiki-header">
                    <div class="quick-wiki-mark" aria-hidden="true">W</div>
                    <div>
                        <h2>${_escapeHtml(_t('quickWiki'))}</h2>
                        <span class="quick-wiki-label">${_escapeHtml(quick.label || 'Summary')}</span>
                    </div>
                    <span class="no-ai-badge">${_escapeHtml(_t('noGenerativeAi'))}</span>
                </div>
                <div class="quick-wiki-body">
                    ${imageHtml}
                    <div class="quick-wiki-text">
                        <div class="quick-wiki-answer">${_escapeHtml(quick.answer)}</div>
                        ${description}
                    </div>
                </div>
                <div class="answer-meta">
                    <span class="badge">${_escapeHtml(sourceBadge)}</span>
                    <a href="${_escapeHtml(_safeHref(quick.url))}" target="_self">${_escapeHtml(quick.title)}</a>
                    ${property}<span>${_escapeHtml(quick.note || _t('openData'))}</span>
                    ${linksHtml ? '<span class="answer-sep">·</span>' + linksHtml : ''}
                </div>
            </div>`;
    }
    window.renderQuickWiki = renderQuickWiki;

    window._gwRender = function() {
        const filtered = allResults;

        if ($('resultStats')) {
            const secs = window._gwSearchMs != null ? (window._gwSearchMs / 1000).toFixed(2) : null;
            $('resultStats').textContent = _t('resultsStats', { count: filtered.length })
                + (secs != null ? ` (${secs} seconds)` : '');
        }

        renderDidYouMean();
        renderQuickWiki();

        const list = $('resultsList');
        const empty = $('emptyState');
        const loadMore = $('loadMoreArea');
        if (!list) return;

        if (!filtered.length) {
            list.innerHTML = '';
            if (empty) empty.style.display = '';
            if (loadMore) loadMore.style.display = 'none';
            return;
        }
        if (empty) empty.style.display = 'none';

        window._gwLoaded = window._gwLoaded || 0;
        const start = window._gwLoaded;
        // An XPDevs query is a fixed, known-size set: show the profile, the
        // site and every repository in one pass instead of 20-row pages.
        const pageSize = _isXpdevsQuery(lastQuery) ? 120 : PER_PAGE;
        const end = Math.min(start + pageSize, filtered.length);
        const newItems = filtered.slice(start, end);

        if (start === 0) {
            list.innerHTML = newItems.map(renderItem).join('');
            window.scrollTo(0, 0);
        } else {
            list.insertAdjacentHTML('beforeend', newItems.map(renderItem).join(''));
        }

        window._gwLoaded = end;
        if (loadMore) {
            loadMore.style.display = (window._gwLoaded < filtered.length) ? 'flex' : 'none';
        }
    };

    window.loadMoreResults = function() { window._gwRender(); };

    // ======================== LUCKY SEARCH ========================

    window.luckySearch = function() {
        const input = $('mainSearchInput');
        if (!input) return;
        const val = input.value.trim();
        if (!val) return;
        performSearch('mainSearchInput').then(completed => {
            if (!completed || lastQuery !== val || !allResults.length) return;
            const target = _safeHref(allResults[0].url);
            if (target !== '#') window.open(target, '_self');
        });
    };

    window.searchSuggestion = function(term) {
        if ($('mainSearchInput')) $('mainSearchInput').value = term;
        if ($('resultsSearchInput')) $('resultsSearchInput').value = term;
        updateClearBtn('mainSearchInput');
        updateClearBtn('resultsSearchInput');
        performSearch('resultsSearchInput');
    };

    window.useQuickWikiExample = function() {
        if (!$('mainSearchInput')) return;
        // The user explicitly asked for a Quick Wiki answer.
        if (!_quickWikiEnabled) applyQuickWiki(true);
        $('mainSearchInput').value = 'release date of PlayStation 5';
        updateClearBtn('mainSearchInput');
        performSearch('mainSearchInput');
    };

    // ======================== KEY HANDLING ========================

    document.addEventListener('keydown', function(e) {
        if (e.key === 'Escape') {
            if ($('settingsOverlay') && $('settingsOverlay').classList.contains('show')) {
                closeSettings(); return;
            }
            if ($('resultsUI') && $('resultsUI').classList.contains('visible')) {
                if ($('resultsSearchInput')) $('resultsSearchInput').focus();
            } else {
                if ($('mainSearchInput')) $('mainSearchInput').focus();
            }
        }
        if (e.key === '/' && !['INPUT', 'TEXTAREA'].includes(e.target.tagName)) {
            e.preventDefault();
            const input = ($('resultsUI') && $('resultsUI').classList.contains('visible')) ? $('resultsSearchInput') : $('mainSearchInput');
            if (input) input.focus();
        }
    });

    // ======================== INIT ========================

    if ($('mainSearchInput')) {
        $('mainSearchInput').addEventListener('keydown', e => { if (e.key === 'Enter') doSearch('mainSearchInput'); });
        $('mainSearchInput').addEventListener('input', () => { searchRequestId++; updateClearBtn('mainSearchInput'); });
    }
    if ($('resultsSearchInput')) {
        $('resultsSearchInput').addEventListener('keydown', e => { if (e.key === 'Enter') doSearch('resultsSearchInput'); });
        $('resultsSearchInput').addEventListener('input', () => { searchRequestId++; updateClearBtn('resultsSearchInput'); });
    }

    initAutocomplete('mainSearchInput', 'homeAutocomplete');
    initAutocomplete('resultsSearchInput', 'resultsAutocomplete');

    function getQueryParam() {
        const params = new URLSearchParams(window.location.search);
        return params.get('query') || params.get('q') || '';
    }

    window.addEventListener('popstate', function() {
        const q = getQueryParam();
        if ($('resultsUI') && $('resultsUI').classList.contains('visible')) {
            if (q) {
                if ($('mainSearchInput')) $('mainSearchInput').value = q;
                if ($('resultsSearchInput')) $('resultsSearchInput').value = q;
                performSearch('resultsSearchInput');
            } else {
                location.reload();
            }
        }
    });

    try {
        const q = getQueryParam();
        if (q) {
            if ($('mainSearchInput')) $('mainSearchInput').value = q;
            if ($('resultsSearchInput')) $('resultsSearchInput').value = q;
            performSearch('mainSearchInput');
        }
    } catch(_) {}

    if ($('indexStats') && typeof window.gatewayIndexSize === 'function') {
        window.gatewayIndexSize().then(count => {
            localIndexCount = count || 0;
            const stats = $('indexStats');
            if (stats) stats.textContent = localIndexCount
                ? _t('indexPages', { count: localIndexCount.toLocaleString(_lang) })
                : _t('indexLoading');
        }).catch(() => {});
    }

    _applyTranslations();
    updateCountry();
    window.setLanguage(_langPreference);
})();

<!--
Copyright (c) 2017, 2019 IBM Corp. and others

Ce programme et le matériel d'accompagnement sont disponibles sous
les termes de la licence publique Eclipse 2.0 qui accompagne ce
distribution et est disponible sur https://www.eclipse.org/legal/epl-2.0/
ou la licence Apache, version 2.0 qui accompagne cette distribution et
est disponible sur https://www.apache.org/licenses/LICENSE-2.0.

Ce code source peut également être mis à disposition sous les
Licences secondaires lorsque les conditions d'une telle disponibilité sont définies
dans la licence publique Eclipse, v. 2.0 sont satisfaits: GNU
Licence publique générale, version 2 avec le chemin de classe GNU
Exception [1] et GNU General Public License, version 2 avec le
Exception d'assemblage OpenJDK [2].

[1] https://www.gnu.org/software/classpath/license.html
[2] http://openjdk.java.net/legal/assembly-exception.html

SPDX-License-Identifier: EPL-2.0 OU Apache-2.0 OU GPL-2.0 AVEC Classpath-exception-2.0 OU LicenseRef-GPL-2.0 WITH Assembly-exception
-->

<p align="center">
<img src="https://github.com/eclipse/openj9/blob/master/artwork/OpenJ9.svg" alt="OpenJ9 logo" align="middle" width="50%" height="50%" />
<p>


Bienvenue dans le répertoire Eclipse OpenJ9
========================================
[![Licence](https://img.shields.io/badge/License-EPL%202.0-green.svg)](https://opensource.org/licenses/EPL-2.0)
[![Licence](https://img.shields.io/badge/License-APL%202.0-green.svg)](https://opensource.org/licenses/Apache-2.0)


Nous ne savons pas quel itinéraire vous pourriez avoir emprunté pour vous rendre ici, mais nous sommes vraiment ravis de vous voir! Si vous venez directement de notre site Web, vous avez probablement déjà beaucoup appris sur Eclipse OpenJ9 et comment il s'intègre à l'écosystème OpenJDK. Si vous êtes venu par un autre itinéraire, voici quelques liens clés pour vous aider à démarrer:

- [SiteWeb Eclipse OpenJ9](http://www.eclipse.org/openj9) - Learn about this high performance, enterprise-grade Java Virtual Machine (JVM) and why we think you want to get involved in its development.
- [SiteWeb AdoptOpenJDK](https://adoptopenjdk.net/releases.html?variant=openjdk9-openj9) - Grab pre-built OpenJDK binaries that embed OpenJ9 and try it out.
- [Instructions de construction](https://www.eclipse.org/openj9/oj9_build.html) - Here's how you can build an OpenJDK with OpenJ9 yourself.
- [FAQ](https://www.eclipse.org/openj9/oj9_faq.html)

Si vous cherchez des moyens d'aider le projet (merci!), Nous avons:
- [Problèmes pour débutants](https://github.com/eclipse/openj9/issues?q=is%3Aopen+is%3Aissue+label%3Abeginner)
- [Aider à résoudre des problèmes existants](https://github.com/eclipse/openj9/issues?utf8=%E2%9C%93&q=is%3Aopen+is%3Aissue+label%3A%22help+wanted%22)

Si vous êtes ici pour en savoir plus sur le projet, lisez la suite ...

Qu'est-ce qu'Eclipse OpenJ9?
=======================

Eclipse OpenJ9 est une implémentation indépendante d'une Java Virtual Machine (machine virtuelle Java). "Mise en œuvre indépendante"
signifie qu'il a été construit en utilisant la spécification Java Virtual Machine sans utiliser de code provenant d'un autre Java
Virtual Machine.

La JVM OpenJ9 se combine avec les bibliothèques de classes Java d'OpenJDK pour créer un JDK complet adapté à
encombrement, performances et fiabilité bien adaptés aux déploiements cloud.

La contribution d'origine à OpenJ9 provenait de la machine virtuelle Java "J9" qui a été utilisée en production
par des milliers d'applications Java au cours des deux dernières décennies. En septembre 2017, IBM a finalisé l'open sourcing
la JVM J9 en tant que "Eclipse OpenJ9" à la Fondation Eclipse. Des parties importantes de J9 sont également open source
au [projet Eclipse OMR](https://github.com/eclipse/omr). OpenJ9 a une licence permissive (Apache
Licence 2.0 ou Eclipse Public License 2.0 avec une licence de compatibilité secondaire pour le projet OpenJDK
Licence GPLv2) conçue pour permettre la création d'OpenJDK avec la machine virtuelle Java OpenJ9. S'il vous plaît voir notre
[Fichier LICENCE](https://github.com/eclipse/openj9/blob/master/LICENSE) pour plus de détails.

Eclipse OpenJ9 est un projet de code source qui peut être construit avec les bibliothèques de classes Java. Plateforme croisée
tous les soirs et libérer les binaires et les conteneurs de docker pour OpenJDK avec OpenJ9, ciblant plusieurs niveaux JDK
(comme JDK8, JDK10, etc.) sont construits par [l'organisation AdoptOpenJDK](https://github.com/adoptopenjdk)
et peut être téléchargé à partir du [site de téléchargement AdoptOpenJDK](https://adoptopenjdk.net/?variant=openjdk8-openj9)
ou sur [DockerHub](https://hub.docker.com/search/?isAutomated=0&isOfficial=0&page=1&pullCount=0&q=openj9&starCount=0).

Quel est l'objectif du projet?
================================

L'objectif à long terme du projet Eclipse OpenJ9 est de favoriser un écosystème ouvert de développeurs JVM capables de collaborer et d'innover avec les concepteurs et développeurs de plates-formes matérielles, de systèmes d'exploitation, d'outils et de frameworks.

Le projet se félicite de la collaboration, embrasse de nouvelles innovations et offre une opportunité d'influencer le développement d'OpenJ9 pour la prochaine génération d'applications Java.

La communauté Java a bénéficié au cours de son histoire de la mise en concurrence de plusieurs implémentations de la spécification JVM pour fournir le meilleur runtime pour votre application. Que ce soit en ajoutant des références compressées, de nouvelles fonctionnalités Cloud, AOT (compilation en avance) ou des performances plus rapides et une utilisation de la mémoire réduite, l'écosystème s'est amélioré grâce à cette concurrence. Eclipse OpenJ9 a pour objectif de continuer à stimuler l'innovation dans l'espace d'exécution.

Comment puis-je contribuer?
====================

Étant donné que nous sommes un projet Eclipse Foundation, chaque contributeur doit signer un accord de contributeur Eclipse. La Fondation Eclipse fonctionne sous le [Code de conduite d'Eclipse] [coc]
promouvoir l'équité, l'ouverture et l'inclusion.

Pour commencer, lisez notre [Guide de contribution](CONTRIBUTING.md).

[coc]: https://eclipse.org/org/documents/Community_Code_of_Conduct.php

Si vous pensez que vous voulez contribuer mais que vous n'êtes pas prêt à signer l'accord de collaboration Eclipse, pourquoi ne pas venir à nos appels hebdomadaires * Demandez à la communauté OpenJ9 * pour en savoir plus sur la façon dont nous travaillons. Nous parlons de nouvelles idées, répondons à toutes les questions qui se posent et discutons des plans et de l'état d'avancement du projet. Nous faisons également des discussions éclair sur les caractéristiques et les fonctions de la machine virtuelle. Visitez le canal * # planning * dans notre [espace de travail Slack] (https://openj9.slack.com/) pour obtenir des informations sur les appels à venir de la communauté et les minutes des réunions précédentes (Rejoignez [ici] (https: //join.slack. com / t / openj9 / shared_invite / enQtNDU4MDI4Mjk0MTk2LWVhNTMzMGY1N2JkODQ1OWE0NTNmZjM4ZDcxOTBiMjk3NGFjM2U0ZDNhMmY0MDZlNzU0Z3AJ)

Quels repértoires font partie du projet?
===================================
- https://github.com/eclipse/openj9: code principal d'OpenJ9
- https://github.com/eclipse/openj9-omr: clone Eclipse OMR pour effectuer des modifications OMR temporaires. (Aucun pour l'instant!)
- https://github.com/eclipse/openj9-systemtest: tests système spécifiques à OpenJ9
- https://github.com/eclipse/openj9-website: Dépôt du site Web OpenJ9
- https://github.com/eclipse/openj9-docs: Dépôt de documentation OpenJ9


Où puis-je en savoir plus?
=======================

Vidéos et présentations
------------------------

- [JavaOne 2017: John Duimovich et Mike Milinkovich s'amusent à discuter d'Eclipse OpenJ9 (et EE4J)] (https://www.youtube.com/watch?v=4g9SdVCPlnk)
- [JavaOne 2017: Holly Cummins interviewant Dan Heidinga et Mark Stoodley sur Eclipse OpenJ9 et OMR] (https://www.youtube.com/watch?v=c1LVXqD3cII)
- [JavaOne 2017: Open sourcing de la machine virtuelle Java J9 IBM] (https://www.slideshare.net/MarkStoodley/javaone-2017-mark-stoodley-open-sourcing-ibm-j9-jvm)
- [JavaOne 2017: Eclipse OpenJ9 sous le capot de la prochaine JVM open source] (https://www.slideshare.net/DanHeidinga/javaone-2017-eclipse-openj9-under-the-hood-of-the-jvm)
- [JavaOne 2017: demandez aux architectes OpenJ9] (https://www.youtube.com/watch?v=qb5ennM_pgc)

Also check out the [Resources] (https://www.eclipse.org/openj9/oj9_resources.html) page on our website.

Copyright (c) 2017, 2018 IBM Corp. and others

Translated by: MJARDI Yassine
